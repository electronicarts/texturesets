// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetCompilingManager.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "AsyncCompilationHelpers.h"
#include "Misc/QueuedThreadPoolWrapper.h"
#include "ObjectCacheContext.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "TextureSet.h"
#include "TextureSetCompiler.h"
#include "TextureSetDefinition.h"
#include "EditorSupportDelegates.h"

#define LOCTEXT_NAMESPACE "TextureSets"

DEFINE_LOG_CATEGORY(LogTextureSet);

static AsyncCompilationHelpers::FAsyncCompilationStandardCVars CVarAsyncTextureSetStandard(
	TEXT("TextureSet"),
	TEXT("texturesets"),
	FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			FTextureSetCompilingManager::Get().FinishAllCompilation();
		}
));

static TAutoConsoleVariable<int> CVarMaxAsyncTextureSetParallelCompiles(
	TEXT("ts.MaxAsyncTextureSetParallelCompiles"),
	4,
	TEXT("Maximum number of async texture set compilations. Mainly used to limit memory usage"));


namespace TextureSetCompilingManagerImpl
{
	static void EnsureInitializedCVars()
	{
		static bool bIsInitialized = false;

		if (!bIsInitialized)
		{
			bIsInitialized = true;

			CVarAsyncTextureSetStandard.AsyncCompilation->Set(true); // Default to true

			AsyncCompilationHelpers::EnsureInitializedCVars(
				TEXT("textureset"),
				CVarAsyncTextureSetStandard.AsyncCompilation,
				CVarAsyncTextureSetStandard.AsyncCompilationMaxConcurrency);
		}
	}
}

FTextureSetCompilingManager::FTextureSetCompilingManager()
	: Notification(GetAssetNameFormat())
{
	TextureSetCompilingManagerImpl::EnsureInitializedCVars();
	FAssetCompilingManager::Get().RegisterManager(this);
}

FName FTextureSetCompilingManager::GetStaticAssetTypeName()
{
	return TEXT("UE-TextureSet");
}

FName FTextureSetCompilingManager::GetAssetTypeName() const
{
	return GetStaticAssetTypeName();
}

TArrayView<FName> FTextureSetCompilingManager::GetDependentTypeNames() const
{
	return TArrayView<FName>{ };
}

FTextFormat FTextureSetCompilingManager::GetAssetNameFormat() const
{
	return LOCTEXT("TextureSetNameFormat", "{0}|plural(one=TextureSet,other=TextureSets)");
}

FQueuedThreadPool* FTextureSetCompilingManager::GetThreadPool() const
{
	static FQueuedThreadPoolWrapper* GTextureThreadPool = nullptr;
	if (GTextureThreadPool == nullptr && FAssetCompilingManager::Get().GetThreadPool() != nullptr)
	{
		const auto PriorityMapper = [](EQueuedWorkPriority TexturePriority) { return FMath::Max(TexturePriority, EQueuedWorkPriority::Low); };

		// Textures will be scheduled on the asset thread pool, where concurrency limits might by dynamically adjusted depending on memory constraints.
		GTextureThreadPool = new FQueuedThreadPoolWrapper(FAssetCompilingManager::Get().GetThreadPool(), -1, PriorityMapper);

		AsyncCompilationHelpers::BindThreadPoolToCVar(
			GTextureThreadPool,
			CVarAsyncTextureSetStandard.AsyncCompilation,
			CVarAsyncTextureSetStandard.AsyncCompilationResume,
			CVarAsyncTextureSetStandard.AsyncCompilationMaxConcurrency
		);
	}

	return GTextureThreadPool;
}

void FTextureSetCompilingManager::Shutdown()
{
	bHasShutdown = true;
	if (CompilingTextureSets.Num() > 0)
	{
		TArray<UTextureSet*> PendingTextureSets;
		PendingTextureSets.Reserve(CompilingTextureSets.Num());

		for (UTextureSet* TextureSet : CompilingTextureSets)
		{
			if (!TryCancelCompilation(TextureSet))
			{
				PendingTextureSets.Add(TextureSet);
			}
		}

		// Wait on texture sets already in progress we couldn't cancel
		FinishCompilation(PendingTextureSets);
	}
}

TRACE_DECLARE_INT_COUNTER(QueuedTextureSetCompilation, TEXT("AsyncCompilation/QueuedTextureSets"));
void FTextureSetCompilingManager::UpdateCompilationNotification()
{
	TRACE_COUNTER_SET(QueuedTextureSetCompilation, GetNumRemainingAssets());
	Notification.Update(GetNumRemainingAssets());
}

bool FTextureSetCompilingManager::IsAsyncCompilationAllowed(UTextureSet* TextureSet) const
{
	check(IsValid(TextureSet->Definition));

	if (TextureSet->Definition->GetDefaultTextureSet() == TextureSet)
		return false; // Default textures must compile immediately
	else if (bHasShutdown || !FPlatformProcess::SupportsMultithreading())
		return false;
	else
		return CVarAsyncTextureSetStandard.AsyncCompilation.GetValueOnAnyThread() != 0;
}

FTextureSetCompilingManager& FTextureSetCompilingManager::Get()
{
	static FTextureSetCompilingManager Singleton;
	return Singleton;
}

void FTextureSetCompilingManager::QueueCompilation(UTextureSet* const InTextureSet)
{
	QueuedTextureSets.Add(InTextureSet);

	// Swap out material instances with default values until compiling has finished
	NotifyMaterialInstances({InTextureSet});
}

void FTextureSetCompilingManager::StartCompilation(UTextureSet* const TextureSet)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::StartCompilation)
	check(IsInGameThread());

	if (CompilingTextureSets.Contains(TextureSet))
	{
		// Only log, because warnings count as a build failure on Hopper
		// TODO: Upgrade to a warning when we are confident this is no-longer happening
		UE_LOG(LogTextureSet, Log, TEXT("Texture Set %s is already compiling so cannot be started. If you are unsure, call IsRegistered(), TryCancelCompilation() and/or FinishCompilation() before attempting to queue a texture set."), *TextureSet->GetName());
	}
	QueuedTextureSets.Remove(TextureSet);

	// Cancel or finish previous compilation
	if (CompilingTextureSets.Contains(TextureSet) && !TryCancelCompilation(TextureSet))
	{
		FinishCompilation({TextureSet});
	}

	// Should really be gone now unless cancel/finish don't work properly
	check(!CompilingTextureSets.Contains(TextureSet));

	TSharedRef<FTextureSetCompiler> Compiler = GetOrCreateCompiler(TextureSet);

	if (Compiler->CompilationRequired())
	{
		CompilingTextureSets.Emplace(TextureSet);
		UE_LOG(LogTextureSet, Log, TEXT("Texture Set %s compilation started"), *TextureSet->GetName());

		if (IsAsyncCompilationAllowed(TextureSet))
			Compiler->ExecuteAsync();
		else
			Compiler->Execute();

		// Will either update the materials to the new values, or swap out material instances with default values until compiling has finished
		NotifyMaterialInstances({TextureSet});
	}

	TRACE_COUNTER_SET(QueuedTextureSetCompilation, GetNumRemainingAssets());
}

void FTextureSetCompilingManager::FinishCompilation(TArrayView<UTextureSet* const> InTextureSets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::FinishCompilation);

	check(IsInGameThread());

	TMap<TWeakObjectPtr<UTextureSet>, TSharedRef<FTextureSetCompiler>> PendingTextureSets;
	PendingTextureSets.Reserve(InTextureSets.Num());

	for (UTextureSet* TextureSet : InTextureSets)
	{
		if (QueuedTextureSets.Contains(TextureSet))
		{
			StartCompilation(TextureSet);
		}

		if (CompilingTextureSets.Contains(TextureSet))
		{
			PendingTextureSets.Add(TextureSet, Compilers.FindChecked(TextureSet));
		}
	}

	if (PendingTextureSets.Num())
	{
		class FCompilableTextureSet final : public AsyncCompilationHelpers::ICompilable
		{
		public:
			FCompilableTextureSet(TPair<TWeakObjectPtr<UTextureSet>, TSharedRef<FTextureSetCompiler>> Args)
				: TextureSet(Args.Get<TWeakObjectPtr<UTextureSet>>().Get())
				, Compiler(Args.Get<TSharedRef<FTextureSetCompiler>>())
			{
				
			}

			FAsyncTaskBase* GetAsyncTask()
			{
				return Compiler->GetAsyncTask();
			}

			virtual void Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority) override final
			{
				if (FAsyncTaskBase* AsyncTask = GetAsyncTask())
					AsyncTask->SetPriority(InPriority);
			}

			virtual bool WaitCompletionWithTimeout(float TimeLimitSeconds) override final
			{
				FTextureSetCompilingManager::Get().ProcessTextureSets(true);
				// Wait until all compilers have cleared
				return !FTextureSetCompilingManager::Get().Compilers.Contains(TextureSet.Get());
			}

			virtual FName GetName() override { return Compiler->GetTextureSetName(); }

			TStrongObjectPtr<UTextureSet> TextureSet;
			TSharedRef<FTextureSetCompiler> Compiler;

		};

		TArray<FCompilableTextureSet> CompilableTextures(PendingTextureSets.Array());

		using namespace AsyncCompilationHelpers;
		FObjectCacheContextScope ObjectCacheScope;
		AsyncCompilationHelpers::FinishCompilation(
			[&CompilableTextures](int32 Index)	-> ICompilable& { return CompilableTextures[Index]; },
			CompilableTextures.Num(),
			LOCTEXT("TextureSets", "TextureSets"),
			LogTextureSet,
			[this](ICompilable* Object)
			{
				// Don't need to do anything here, as WaitCompletionWithTimeout calls ProcessTextureSets, which will remove them form the list
			}
		);
	}
}

bool FTextureSetCompilingManager::TryCancelCompilation(UTextureSet* const TextureSet)
{
	check(IsInGameThread());

	if (!CompilingTextureSets.Contains(TextureSet))
		return true;

	TSharedRef<FTextureSetCompiler> Compiler = Compilers.FindChecked(TextureSet);
	if (Compiler->TryCancel())
	{
		CompilingTextureSets.Remove(TextureSet);
		UE_LOG(LogTextureSet, Log, TEXT("Texture Set %s cancelled"), *TextureSet->GetName());
		return true;
	}
	else
	{
		return false;
	}
}

bool FTextureSetCompilingManager::IsRegistered(const UTextureSet* TextureSet) const
{
	check(IsInGameThread());
	return CompilingTextureSets.Contains(TextureSet) || QueuedTextureSets.Contains(TextureSet);
}

void FTextureSetCompilingManager::NotifyMaterialInstances(TArrayView<UTextureSet* const> InTextureSets)
{
	check(IsInGameThread());

	if (FApp::CanEverRender())
	{
		for (const UTextureSet* TextureSet : InTextureSets)
		{
			// Update loaded material instances that use this texture set
			MaterialInstancesToUpdate.Add(TextureSet);
		}
	}
}

TSharedRef<FTextureSetCompiler> FTextureSetCompilingManager::GetOrCreateCompiler(UTextureSet* TextureSet)
{
	check(IsInGameThread());

	if (!Compilers.Contains(TextureSet))
	{
		TSharedRef<FTextureSetCompiler> Compiler = MakeShared<FTextureSetCompiler>(TextureSet, TextureSet->DerivedData);
		Compilers.Add(TextureSet, Compiler);
		// TODO: Prepare compiler ony if needed
		Compiler->Prepare();
		return Compiler;
	}
	else
	{
		return Compilers.FindChecked(TextureSet);
	}
}

void FTextureSetCompilingManager::FinishAllCompilation()
{
	UE_SCOPED_ENGINE_ACTIVITY(TEXT("Finish All TextureSet Compilation"));

	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::FinishAllCompilation)

	if (CompilingTextureSets.Num() > 0)
	{
		TArray<UTextureSet*> PendingTextureSets;
		PendingTextureSets.Reserve(CompilingTextureSets.Num());

		for (UTextureSet* TextureSet : CompilingTextureSets)
		{
			check(IsValid(TextureSet));
			PendingTextureSets.Add(TextureSet);
		}

		FinishCompilation(PendingTextureSets);
	}
}

TSharedRef<FTextureSetCompiler> FTextureSetCompilingManager::BorrowCompiler(UTextureSet* InTextureSet)
{
	check(IsValid(InTextureSet));

	int& NumBorrowers = LentCompilers.FindOrAdd(InTextureSet);
	NumBorrowers++;

	if (!Compilers.Contains(InTextureSet))
	{
		check(IsInGameThread());
		return GetOrCreateCompiler(InTextureSet);
	}
	else
	{
		return Compilers.FindChecked(InTextureSet);
	}
}

void FTextureSetCompilingManager::ReturnCompiler(UTextureSet* InTextureSet)
{
	check(IsValid(InTextureSet));

	int& NumBorrowers = LentCompilers.FindOrAdd(InTextureSet);
	check(NumBorrowers > 0);
	NumBorrowers--;
}

void FTextureSetCompilingManager::ProcessTextureSets(bool bLimitExecutionTime)
{
	check(IsInGameThread());

	using namespace TextureSetCompilingManagerImpl;
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::ProcessTextures);

	const double TickStartTime = FPlatformTime::Seconds();

	auto HasTimeLeft = [TickStartTime, bLimitExecutionTime]() -> bool {
		const double MaxSecondsPerFrame = 0.016;
		return bLimitExecutionTime ? ((FPlatformTime::Seconds() - TickStartTime) < MaxSecondsPerFrame) : true;
	};

	if (CompilingTextureSets.Num() > 0)
	{
		FObjectCacheContextScope ObjectCacheScope;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ProcessFinishedTextures);

			TArray<UTextureSet*> TextureSetsToPostpone;
			for (UTextureSet* TextureSet : CompilingTextureSets)
			{
				check(IsValid(TextureSet));

				TSharedRef<FTextureSetCompiler> Compiler = Compilers.FindChecked(TextureSet);

				// HasTimeLeft() ensures we don't stall the editor if too many texture sets finish at the same time
				if (!HasTimeLeft() || Compiler->IsAsyncJobInProgress())
				{
					TextureSetsToPostpone.Emplace(MoveTemp(TextureSet));
				}
				else
				{
					UE_LOG(LogTextureSet, Log, TEXT("Texture Set %s compiled"), *TextureSet->GetName());
				}
			}

			CompilingTextureSets = MoveTemp(TextureSetsToPostpone);
		}
	}

	// Clean up any compilers that are no longer needed
	TArray<UTextureSet*> CompilersToDelete;
	TArray<TWeakObjectPtr<UTextureSet>> StaleCompilers;
	for (auto& [TextureSet, Compiler] : Compilers)
	{
		if (!TextureSet.IsValid())
		{
			UE_LOG(LogTextureSet, Display, TEXT("Texture set %s in Compiler collection was GCed. Removing."), Compiler->GetTextureSetName());
			StaleCompilers.Add(TextureSet);
			continue;
		}
		
		if (!CompilingTextureSets.Contains(TextureSet))
		{
			bool CanDelete = true;

			if (FApp::CanEverRender())
			{
				for (UTexture* Texture : TextureSet->GetDerivedData().Textures)
				{
					// It's finicky to get the texture to finish with a valid, non-default resource;
					// Wait until we're not doing any async work
					if (Texture->IsAsyncCacheComplete()) 
					{
						// Need to explicitly call FinishCachePlatformData otherwise we may have a texture stuck in limbo
						Texture->FinishCachePlatformData();
						// UpdateResource needs to be called AFTER FinishCachePlatformData
						// Otherwise it just kicks off a new build and sets the resource to a default texture
						Texture->UpdateResource();
					}
					
					if (Texture->IsDefaultTexture())
					{
						CanDelete = false;
					}
				}
			}

			if (LentCompilers.FindOrAdd(TextureSet.Get()) > 0)
				CanDelete = false;

			if (CanDelete)
			{
				CompilersToDelete.Add(TextureSet.Get());
			}
		}
	}

	for (UTextureSet* TextureSet : CompilersToDelete)
	{
		UE_LOG(LogTextureSet, Verbose, TEXT("Texture set %s is ready"), *TextureSet->GetName());
		Compilers.Remove(TextureSet);
	}

	NotifyMaterialInstances({CompilersToDelete});

	for (TWeakObjectPtr<UTextureSet> StalePointer : StaleCompilers)
	{
		Compilers.Remove(StalePointer);
	}

	const int32 MaxParallel = CVarMaxAsyncTextureSetParallelCompiles.GetValueOnGameThread();

	if (QueuedTextureSets.Num() > 0 && CompilingTextureSets.Num() < MaxParallel)
	{
		TArray<UTextureSet*> TextureSetsToStart;
		TArray<UTextureSet*> TextureSetsToDequeue;
		TextureSetsToStart.Reserve(FMath::Min(QueuedTextureSets.Num(), MaxParallel));

		for (TSoftObjectPtr<UTextureSet> QueuedTextureSet : QueuedTextureSets)
		{
			if (!HasTimeLeft() || (CompilingTextureSets.Num() + TextureSetsToStart.Num()) >= MaxParallel)
				break;

			UTextureSet* TextureSet = QueuedTextureSet.Get();
			
			if (!IsValid(TextureSet))
			{
				// Texture Set may be null if deleted or garbage collected
				TextureSetsToDequeue.Add(TextureSet);
			}
			else if (!CompilingTextureSets.Contains(TextureSet) || TryCancelCompilation(TextureSet))
			{
				// The texture set is not currently compiling, or was but the async job could be cancelled, so we are safe to kick it off.
				TextureSetsToStart.Add(TextureSet);
			}
		}

		for (UTextureSet* TextureSet : TextureSetsToDequeue)
		{
			QueuedTextureSets.Remove(TextureSet);
		}

		for (UTextureSet* TextureSet : TextureSetsToStart)
		{
			// StartCompilation will remove the texture set from the queue
			StartCompilation(TextureSet);
		}
	}
}

void FTextureSetCompilingManager::RefreshMaterialInstances()
{
	// Refresh all material instances that need to be, and invalidate the viewport
	TSet<const UTextureSet*> PostPonedMaterialInstances;
	if (!MaterialInstancesToUpdate.IsEmpty())
	{
		for (TObjectIterator<UMaterialInstance> It; It; ++It)
		{
			for (FCustomParameterValue& Param : It->CustomParameterValues)
			{
				const UTextureSet* TextureSet = Cast<UTextureSet>(Param.ParameterValue);
				if (IsValid(TextureSet) && MaterialInstancesToUpdate.Contains(TextureSet))
				{
					FPropertyChangedEvent Event(nullptr);
					if (AllDependenciesLoaded(*It))
					{
						It->PostEditChangeProperty(Event);
					}
					else
					{
						PostPonedMaterialInstances.Add(TextureSet);
					}

					break; // Don't need to check other properties, since we've already refreshed this one
				}
			}
		}

		MaterialInstancesToUpdate = MoveTemp(PostPonedMaterialInstances);
		
		// Update all the viewports
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	}
}

bool FTextureSetCompilingManager::AllDependenciesLoaded(UMaterialInstance* MaterialInstance)
{
	FMaterialLayersFunctions Functions;
	MaterialInstance->GetMaterialLayers(Functions);
	for(TObjectPtr<class UMaterialFunctionInterface> BlendFunction : Functions.Blends)
	{
		if (BlendFunction && BlendFunction->HasAnyFlags(RF_NeedPostLoad))
		{
			UE_LOG(LogTextureSet, Log, TEXT("Skipping material update for '%s' because dependency '%s' was not loaded."), *MaterialInstance->GetName(), *BlendFunction->GetName());
			return false;			
		}
	}

	for (TObjectPtr<class UMaterialFunctionInterface> LayerFunction : Functions.Layers)
	{
		if (LayerFunction && LayerFunction->HasAnyFlags(RF_NeedPostLoad))
		{
			UE_LOG(LogTextureSet, Log, TEXT("Skipping material update for '%s' because dependency '%s' was not loaded."), *MaterialInstance->GetName(), *LayerFunction->GetName());
			return false;
		}
	}

	return true;	
}

int32 FTextureSetCompilingManager::GetNumRemainingAssets() const
{
	check(IsInGameThread());
	return Compilers.Num() + QueuedTextureSets.Num();
}

void FTextureSetCompilingManager::ProcessAsyncTasks(bool bLimitExecutionTime)
{
	FObjectCacheContextScope ObjectCacheScope;

	ProcessTextureSets(bLimitExecutionTime);

	RefreshMaterialInstances();

	UpdateCompilationNotification();
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
