// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetCompilingManager.h"

#include "Materials/MaterialFunctionInstance.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "AsyncCompilationHelpers.h"
#include "EditorSupportDelegates.h"
#include "Misc/QueuedThreadPoolWrapper.h"
#include "ObjectCacheContext.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "TextureSet.h"
#include "TextureSetCompiler.h"
#include "TextureSetDefinition.h"
#include "TextureSetTextureSourceProvider.h"
#include "TextureSetsHelpers.h"

#define LOCTEXT_NAMESPACE "TextureSets"

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
	32,
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

// Copy the code of FMemoryBoundQueuedThreadPoolWrapper::GetMemoryLimit() to prevent a divergence and since it'll probably be temp code until we integrate latest refactor from mpalko
int64 TextureSetManagerGetMemoryLimit()
{
	const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();

	// UsedPhysical = "working set"
	// UsedVirtual  = "commit charge"
	UE_LOG(LogTextureSet, Verbose, TEXT("GetMemoryLimit UsedPhysical = %.3f MB (Peak %.3f MB) UsedVirtual = %.3f MB (Peak %.3f MB) AvailPhysical = %.3f MB AvailVirtual = %.3f MB "),
		MemoryStats.UsedPhysical / (1024 * 1024.f),
		MemoryStats.PeakUsedPhysical / (1024 * 1024.f),
		MemoryStats.UsedVirtual / (1024 * 1024.f),
		MemoryStats.PeakUsedVirtual / (1024 * 1024.f),
		MemoryStats.AvailablePhysical / (1024 * 1024.f),
		MemoryStats.AvailableVirtual / (1024 * 1024.f));

	// Just make sure available physical fits in a int64, if it's bigger than that, we're not expecting to be memory limited anyway
	// Also uses AvailableVirtual because the system might have plenty of physical memory but still be limited by virtual memory available in some cases.
	//   (i.e. per-process quota, paging file size lower than actual memory available, etc.).
	int64 AvailableMemory = (int64)FMath::Min3(MemoryStats.AvailablePhysical, MemoryStats.AvailableVirtual, (uint64)INT64_MAX);

	const int64 LeaveMemoryHeadRoom = 64 * 1024 * 1024; // leave 64 MB head room
	AvailableMemory = FMath::Max(0, AvailableMemory - LeaveMemoryHeadRoom);

	return AvailableMemory;
}

void FTextureSetCompilingManager::Shutdown()
{
	bHasShutdown = true;
	if (AsyncCompilationTasks.Num() > 0)
	{
		TArray<UTextureSet*> InFlightTasks;
		AsyncCompilationTasks.GetKeys(InFlightTasks);

		TArray<UTextureSet*> PendingTextureSets;
		PendingTextureSets.Reserve(InFlightTasks.Num());

		for (UTextureSet* TextureSet : InFlightTasks)
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

bool FTextureSetCompilingManager::IsAsyncCompilationAllowed() const
{
	if (bHasShutdown || !FPlatformProcess::SupportsMultithreading())
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
	check(!InTextureSet->IsDefaultTextureSet());

	QueuedTextureSets.Add(InTextureSet);

	// Swap out material instances with default values until compiling has finished
	if (InTextureSet->DerivedData != nullptr)
	{
		InTextureSet->DerivedData = nullptr;
		NotifyMaterialInstances({InTextureSet});
	}
}

void FTextureSetCompilingManager::StartCompilation(UTextureSet* const TextureSet, bool bAsync)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::StartCompilation)
	check(IsInGameThread());

	QueuedTextureSets.Remove(TextureSet);

	TSharedRef<FTextureSetCompiler> Compiler = MakeShared<FTextureSetCompiler>(MakeCompilerArgs(TextureSet));
	TSharedPtr<TextureSetCompilerTask>* ExistingTask = AsyncCompilationTasks.Find(TextureSet);

	if (ExistingTask)
	{
		if (ExistingTask->Get()->GetCompiler()->Equivalent(Compiler.Get()))
		{
			// New compiler would produce the same result as the existing in-flight task
			if (bAsync)
			{
				// Nothing to do here, as the task will finish eventually will the data we need.
				return;
			}
			else
			{
				// Make sure we finish the current job since we're not running async
				FinishCompilation({TextureSet});
				return;
			}
		}
		else
		{
			UE_LOG(LogTextureSet, Warning, TEXT("%s: is already compiling, but data has changed. Trying to cancel or finish previous job and restarting with updated data."), *TextureSet->GetName());

			// Cancel or finish previous compilation, as it's outdated
			if (!TryCancelCompilation(TextureSet))
				FinishCompilation({TextureSet});
		}
	}

	// Should really be gone now unless cancel/finish don't work properly
	check(!AsyncCompilationTasks.Contains(TextureSet));

	if (Compiler->CompilationRequired(TextureSet->DerivedData.Get()))
	{
		TSharedPtr<TextureSetCompilerTask> Task = MakeShared<TextureSetCompilerTask>(Compiler, TextureSet->IsDefaultTextureSet());

		if (bAsync && IsAsyncCompilationAllowed())
		{
			UE_LOG(LogTextureSet, Log, TEXT("%s: starting async compilation"), *TextureSet->GetName());

			Task->StartAsync(GetThreadPool(), EQueuedWorkPriority::Normal);

			AsyncCompilationTasks.Add(TextureSet, Task);

			// Swap out material instances with default values until compiling has finished
			if (TextureSet->DerivedData != nullptr)
			{
				TextureSet->DerivedData = nullptr;
				NotifyMaterialInstances({TextureSet});
			}
		}
		else
		{
			Task->Start();
			Task->Finalize();
			AssignDerivedData(Task->GetDerivedData(), TextureSet);
			NotifyMaterialInstances({TextureSet});

			UE_LOG(LogTextureSet, Log, TEXT("%s: compiled"), *TextureSet->GetName());
		}
	}

	TRACE_COUNTER_SET(QueuedTextureSetCompilation, GetNumRemainingAssets());
}

void FTextureSetCompilingManager::FinishCompilation(TArrayView<UTextureSet* const> InTextureSets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::FinishCompilation);

	check(IsInGameThread());

	class FCompilableTextureSet final : public AsyncCompilationHelpers::ICompilable
	{
	public:
		FCompilableTextureSet(UTextureSet* TextureSet, TSharedPtr<TextureSetCompilerTask> Task)
			: TextureSet(TextureSet)
			, Task(Task)
		{}

		virtual void Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority) override final
		{
			Task->SetPriority(InPriority);
		}

		virtual bool WaitCompletionWithTimeout(float TimeLimitSeconds) override final
		{
			return Task->TryFinalize();
		}

		virtual FName GetName() override final { return  TextureSet->GetFName(); }

		TStrongObjectPtr<UTextureSet> TextureSet;
		TSharedPtr<TextureSetCompilerTask> Task;
	};

	TArray<FCompilableTextureSet> CompilableTextures;
	CompilableTextures.Reserve(InTextureSets.Num());

	for (UTextureSet* TextureSet : InTextureSets)
	{
		check(AsyncCompilationTasks.Contains(TextureSet));
		CompilableTextures.Add(FCompilableTextureSet(TextureSet, AsyncCompilationTasks.FindAndRemoveChecked(TextureSet)));
	}

	if (CompilableTextures.Num() > 0)
	{
		using namespace AsyncCompilationHelpers;
		FObjectCacheContextScope ObjectCacheScope;
		AsyncCompilationHelpers::FinishCompilation(
			[&CompilableTextures](int32 Index)	-> ICompilable& { return CompilableTextures[Index]; },
			CompilableTextures.Num(),
			LOCTEXT("TextureSets", "TextureSets"),
			LogTextureSet,
			[this](ICompilable* Object)
			{
				FCompilableTextureSet* Compilable = (FCompilableTextureSet*)Object;

				// Remove the task, and move it's derived data into the texture set
				AssignDerivedData(Compilable->Task->GetDerivedData(), Compilable->TextureSet.Get());

				UE_LOG(LogTextureSet, Log, TEXT("%s: finished async compilation"), *Compilable->TextureSet->GetName());
			}
		);
	}
}

bool FTextureSetCompilingManager::TryCancelCompilation(UTextureSet* const TextureSet)
{
	check(IsInGameThread());

	if (!AsyncCompilationTasks.Contains(TextureSet))
		return true;

	TSharedPtr<TextureSetCompilerTask> Task = AsyncCompilationTasks.FindChecked(TextureSet);

	if (Task->Cancel())
	{
		AsyncCompilationTasks.Remove(TextureSet);
		UE_LOG(LogTextureSet, Log, TEXT("%s: cancelled compilation"), *TextureSet->GetName());
		return true;
	}
	else
	{
		return false;
	}
}

bool FTextureSetCompilingManager::IsCompiling(const UTextureSet* TextureSet) const
{
	check(IsInGameThread());
	return AsyncCompilationTasks.Contains(TextureSet);
}

bool FTextureSetCompilingManager::IsQueued(const UTextureSet* TextureSet) const
{
	check(IsInGameThread());
	return QueuedTextureSets.Contains(TextureSet);
}

bool FTextureSetCompilingManager::IsRegistered(const UTextureSet* TextureSet) const
{
	check(IsInGameThread());
	return IsQueued(TextureSet) || IsCompiling(TextureSet);
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

TSharedRef<FTextureSetCompilerArgs> FTextureSetCompilingManager::MakeCompilerArgs(UTextureSet* TextureSet)
{
	check(IsInGameThread());
	TSharedRef<FTextureSetCompilerArgs> CompilerArgs = MakeShared<FTextureSetCompilerArgs>();
	CompilerArgs->ModuleInfo = TextureSet->Definition->GetModuleInfo();
	CompilerArgs->PackingInfo = TextureSet->Definition->GetPackingInfo();
	CompilerArgs->SourceTextures = TextureSet->SourceTextures;
	CompilerArgs->AssetParams = TextureSet->AssetParams;
	CompilerArgs->OuterObject = TextureSet;
	CompilerArgs->NamePrefix = TextureSet->GetName();
	CompilerArgs->DebugContext = TextureSet->GetFullName();
	CompilerArgs->UserKey = TextureSet->GetUserKey() + TextureSet->Definition->GetUserKey();
	return CompilerArgs;
}

void FTextureSetCompilingManager::FinishAllCompilation()
{
	UE_SCOPED_ENGINE_ACTIVITY(TEXT("Finish All TextureSet Compilation"));

	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::FinishAllCompilation)

	if (AsyncCompilationTasks.Num() > 0)
	{
		TArray<UTextureSet*> PendingTextureSets;
		PendingTextureSets.Reserve(AsyncCompilationTasks.Num());

		for (auto& [TextureSet, Task] : AsyncCompilationTasks)
			PendingTextureSets.Add(TextureSet);

		FinishCompilation(PendingTextureSets);
	}
}

void FTextureSetCompilingManager::ProcessTextureSets(bool bLimitExecutionTime)
{
	check(IsInGameThread());

	const double TickStartTime = FPlatformTime::Seconds();

	auto HasTimeLeft = [TickStartTime, bLimitExecutionTime]() -> bool {
		const double MaxSecondsPerFrame = 0.016;
		return bLimitExecutionTime ? ((FPlatformTime::Seconds() - TickStartTime) < MaxSecondsPerFrame) : true;
	};

	if (AsyncCompilationTasks.Num() > 0)
	{
		TArray<UTextureSet*> FinishedTextureSets;
		for (auto& [TextureSet, Task] : AsyncCompilationTasks)
		{
			check(IsValid(TextureSet));

			// HasTimeLeft() ensures we don't stall the editor if too many texture sets finish at the same time
			if (HasTimeLeft() && Task->TryFinalize())
			{
				FinishedTextureSets.Add(TextureSet);
			}
		}

		FinishCompilation(FinishedTextureSets);
		NotifyMaterialInstances(FinishedTextureSets);
	}

	int32 MaxParallel = CVarMaxAsyncTextureSetParallelCompiles.GetValueOnGameThread();

	int64 MemoryLimit = TextureSetManagerGetMemoryLimit();
	const uint64 MemoryPerTextureSet = 4ULL * 1024 * 1024 * 1024; // Memory per Texture Set Compilation
	int32 MaximumParallelAllowedByMemory = MemoryLimit / MemoryPerTextureSet;

	MaxParallel = FMath::Min(FMath::Max(1, GLargeThreadPool->GetNumThreads() / 2), MaxParallel);
	MaxParallel = FMath::Min(MaximumParallelAllowedByMemory, MaxParallel);

	if (QueuedTextureSets.Num() > 0 && !IsLoading() && AsyncCompilationTasks.Num() < MaxParallel)
	{
		TArray<UTextureSet*> TextureSetsToStart;
		TArray<TWeakObjectPtr<UTextureSet>> TextureSetsToDequeue;
		TextureSetsToStart.Reserve(FMath::Min(QueuedTextureSets.Num(), MaxParallel));

		for (const TWeakObjectPtr<UTextureSet> QueuedTextureSet : QueuedTextureSets)
		{
			if (!HasTimeLeft())
				break;
			
			if (!QueuedTextureSet.IsValid())
			{
				// Texture Set may be null if deleted or garbage collected
				TextureSetsToDequeue.Add(QueuedTextureSet);
				break;
			}

			UTextureSet* TextureSet = QueuedTextureSet.Get();

			if (!AsyncCompilationTasks.Contains(TextureSet) || TryCancelCompilation(TextureSet))
			{
				// The texture set is not currently compiling, or was but the async job could be cancelled, so we are safe to kick it off.
				TextureSetsToStart.Add(TextureSet);
			}

			// Do not continue starting texture sets if we'll be at our max
			if ((AsyncCompilationTasks.Num() + TextureSetsToStart.Num()) >= MaxParallel)
				break;
		}

		for (const TWeakObjectPtr<UTextureSet>& TextureSet : TextureSetsToDequeue)
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
	TSet<const UTextureSet*> PostponedMaterialInstances;
	if (!MaterialInstancesToUpdate.IsEmpty())
	{
		for (TObjectIterator<UMaterialInstance> It; It; ++It)
		{
			TObjectPtr<const UTextureSet> ContainedTextureSet = nullptr;
			for (FCustomParameterValue& Param : It->CustomParameterValues)
			{
				const UTextureSet* TextureSet = Cast<UTextureSet>(Param.ParameterValue);
				if (IsValid(TextureSet) && MaterialInstancesToUpdate.Contains(TextureSet))
				{
					ContainedTextureSet = TextureSet;
					break; // Don't need to check other properties, since we've already refreshed this one
				}
			}

			// Check if the material instance has overrides for the TS in a layer
			FMaterialLayersFunctions Functions;
			It->GetMaterialLayers(Functions);
			for (int32 LayerIndex = 0;
				LayerIndex < Functions.GetRuntime().Layers.Num() && !IsValid(ContainedTextureSet);
				LayerIndex += 1)
			{
				const TObjectPtr<UMaterialFunctionInstance> LayerInstance
					= IsValid(Functions.GetRuntime().Layers[LayerIndex])
					? Cast<UMaterialFunctionInstance>(Functions.GetRuntime().Layers[LayerIndex])
					: nullptr;
				
				if (!IsValid(LayerInstance))
				{
					continue;
				}

				for (FCustomParameterValue& Param : LayerInstance->CustomParameterValues)
				{
					const UTextureSet* TextureSet = Cast<UTextureSet>(Param.ParameterValue);
					if (IsValid(TextureSet) && MaterialInstancesToUpdate.Contains(TextureSet))
					{
						ContainedTextureSet = TextureSet;
						break; // Don't need to check other properties, since we've already refreshed this one
					}
				}
			}
			
			if (IsValid(ContainedTextureSet))
			{
				if (AllDependenciesLoaded(*It))
				{
					FPropertyChangedEvent Event(nullptr);
					It->PostEditChangeProperty(Event);
				}
				else
				{
					PostponedMaterialInstances.Add(ContainedTextureSet);
				}
			}
		}

		MaterialInstancesToUpdate = MoveTemp(PostponedMaterialInstances);
		
		// Update all the viewports
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	}
}

void FTextureSetCompilingManager::AssignDerivedData(UTextureSetDerivedData* NewDerivedData, UTextureSet* TextureSet)
{
	FString DerivedDataName = TEXT("DerivedData");

	ERenameFlags RenameFlags = REN_DoNotDirty | REN_DontCreateRedirectors;

	// Discard the old derived data if it exists
	UObject* ExistingDerivedData = StaticFindObject(nullptr, TextureSet, *DerivedDataName, true);
	if (ExistingDerivedData)
	{
		ExistingDerivedData->Rename(nullptr, nullptr, RenameFlags);
		ExistingDerivedData->ConditionalBeginDestroy();
	}

	// Reparent the derived data to the texture set
	NewDerivedData->Rename(*DerivedDataName, TextureSet, RenameFlags);
	TextureSet->DerivedData = NewDerivedData;

	// Default texture set derived textures need to be public so they can be referenced as default textures in the generated graphs.
	if (TextureSet->IsDefaultTextureSet())
	{
		NewDerivedData->SetFlags(RF_Public);

		for (FDerivedTexture& DerivedTexture : NewDerivedData->Textures)
			DerivedTexture.Texture->SetFlags(RF_Public);
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
	return AsyncCompilationTasks.Num() + QueuedTextureSets.Num();
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
