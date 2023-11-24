// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetCompilingManager.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "AsyncCompilationHelpers.h"
#include "Misc/QueuedThreadPoolWrapper.h"
#include "ObjectCacheContext.h"
#include "ProfilingDebugging/CookStats.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "TextureSet.h"
#include "TextureSetCooker.h"
#include "TextureSetDefinition.h"

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

static TAutoConsoleVariable<int> CVarMaxAsyncTextureSetCooks(
	TEXT("ts.MaxAsyncTextureSetCooks"),
	4,
	TEXT("Maximum number of async texture set cooks. Mainly used to limit memory usage"));


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
}

void FTextureSetCompilingManager::StartCompilation(UTextureSet* const TextureSet)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::StartCompilation)
	check(IsInGameThread());

	checkf(!CompilingTextureSets.Contains(TextureSet), TEXT("Texture Set is already compiling so cannot be started. If you are unsure, call IsRegistered(), TryCancelCompilation() and/or FinishCompilation() before attempting to queue a texture set."));

	QueuedTextureSets.Remove(TextureSet);

	// Cancel or finish previous compilation
	if (CompilingTextureSets.Contains(TextureSet) && !TryCancelCompilation(TextureSet))
	{
		FinishCompilation({TextureSet});
	}

	// Should really be gone now unless cancel/finish don't work properly
	check(!CompilingTextureSets.Contains(TextureSet));

	TSharedRef<FTextureSetCooker> Cooker = GetOrCreateCooker(TextureSet);

	if (Cooker->CookRequired())
	{
		CompilingTextureSets.Emplace(TextureSet);

		if (IsAsyncCompilationAllowed(TextureSet))
			Cooker->ExecuteAsync();
		else
			Cooker->Execute();

		// Will either update the materials to the new values, or swap out material instances with default values until cooking has finished
		TextureSet->NotifyMaterialInstances();
	}

	TRACE_COUNTER_SET(QueuedTextureSetCompilation, GetNumRemainingAssets());
}

void FTextureSetCompilingManager::FinishCompilation(TArrayView<UTextureSet* const> InTextureSets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::FinishCompilation);

	check(IsInGameThread());

	TMap<TWeakObjectPtr<UTextureSet>, TSharedRef<FTextureSetCooker>> PendingTextureSets;
	PendingTextureSets.Reserve(InTextureSets.Num());

	for (UTextureSet* TextureSet : InTextureSets)
	{
		if (QueuedTextureSets.Contains(TextureSet))
		{
			StartCompilation(TextureSet);
		}

		if (CompilingTextureSets.Contains(TextureSet))
		{
			PendingTextureSets.Add(TextureSet, Cookers.FindChecked(TextureSet));
		}
	}

	if (PendingTextureSets.Num())
	{
		class FCompilableTextureSet final : public AsyncCompilationHelpers::ICompilable
		{
		public:
			FCompilableTextureSet(TPair<TWeakObjectPtr<UTextureSet>, TSharedRef<FTextureSetCooker>> Args)
				: TextureSet(Args.Get<TWeakObjectPtr<UTextureSet>>().Get())
				, Cooker(Args.Get<TSharedRef<FTextureSetCooker>>())
			{
				
			}

			FAsyncTaskBase* GetAsyncTask()
			{
				return Cooker->GetAsyncTask();
			}

			virtual void Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority) override final
			{
				if (FAsyncTaskBase* AsyncTask = GetAsyncTask())
					AsyncTask->SetPriority(InPriority);
			}

			virtual bool WaitCompletionWithTimeout(float TimeLimitSeconds) override final
			{
				FTextureSetCompilingManager::Get().ProcessTextureSets(true);
				// Wait until all cookers have cleared
				return !FTextureSetCompilingManager::Get().Cookers.Contains(TextureSet.Get());
			}

			virtual FName GetName() override { return Cooker->GetTextureSetName(); }

			TStrongObjectPtr<UTextureSet> TextureSet;
			TSharedRef<FTextureSetCooker> Cooker;

		};

		TArray<FCompilableTextureSet> CompilableTextures(PendingTextureSets.Array());

		using namespace AsyncCompilationHelpers;
		FObjectCacheContextScope ObjectCacheScope;
		AsyncCompilationHelpers::FinishCompilation(
			[&CompilableTextures](int32 Index)	-> ICompilable& { return CompilableTextures[Index]; },
			CompilableTextures.Num(),
			LOCTEXT("TextureSets", "TextureSets"),
			LogTexture,
			[this](ICompilable* Object)
			{
				const FCompilableTextureSet* Compilable = static_cast<FCompilableTextureSet*>(Object);

				UTextureSet* TextureSet = Compilable->TextureSet.Get();
				CompilingTextureSets.Remove(TextureSet);
			}
		);
	}
}

bool FTextureSetCompilingManager::TryCancelCompilation(UTextureSet* const TextureSet)
{
	if (!CompilingTextureSets.Contains(TextureSet))
		return true;

	TSharedRef<FTextureSetCooker> Cooker = Cookers.FindChecked(TextureSet);
	if (Cooker->TryCancel())
	{
		CompilingTextureSets.Remove(TextureSet);
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

void FTextureSetCompilingManager::PostCompilation(UTextureSet* TextureSet)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::PostCompilation);

	if (FApp::CanEverRender())
	{
		// Update loaded material instances that use this texture set
		TextureSet->NotifyMaterialInstances();
	}

	UE_LOG(LogTexture, Verbose, TEXT("Texture set %s is ready"), *TextureSet->GetName());
}

TSharedRef<FTextureSetCooker> FTextureSetCompilingManager::GetOrCreateCooker(UTextureSet* TextureSet)
{
	check(IsInGameThread());

	if (!Cookers.Contains(TextureSet))
	{
		TSharedRef<FTextureSetCooker> Cooker = MakeShared<FTextureSetCooker>(TextureSet, TextureSet->DerivedData);
		Cookers.Add(TextureSet, Cooker);
		// TODO: Prepare cooker ony if needed
		Cooker->Prepare();
		return Cooker;
	}
	else
	{
		return Cookers.FindChecked(TextureSet);
	}
}

void FTextureSetCompilingManager::FinishAllCompilation()
{
	UE_SCOPED_ENGINE_ACTIVITY(TEXT("Finish TextureSet Compilation"));

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

TSharedRef<FTextureSetCooker> FTextureSetCompilingManager::BorrowCooker(UTextureSet* InTextureSet)
{
	check(IsValid(InTextureSet));

	int& NumBorrowers = LentCookers.FindOrAdd(InTextureSet);
	NumBorrowers++;

	if (!Cookers.Contains(InTextureSet))
	{
		check(IsInGameThread());
		return GetOrCreateCooker(InTextureSet);
	}
	else
	{
		return Cookers.FindChecked(InTextureSet);
	}
}

void FTextureSetCompilingManager::ReturnCooker(UTextureSet* InTextureSet)
{
	check(IsValid(InTextureSet));

	int& NumBorrowers = LentCookers.FindOrAdd(InTextureSet);
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

				TSharedRef<FTextureSetCooker> Cooker = Cookers.FindChecked(TextureSet);

				// HasTimeLeft() ensures we don't stall the editor if too many texture sets finish at the same time
				if (!HasTimeLeft() || Cooker->IsAsyncJobInProgress())
				{
					TextureSetsToPostpone.Emplace(MoveTemp(TextureSet));
				}
			}

			CompilingTextureSets = MoveTemp(TextureSetsToPostpone);
		}
	}

	// Clean up any cookers that are not needed
	TArray<UTextureSet*> CookersToDelete;
	for (auto& [TextureSet, Cooker] : Cookers)
	{
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

			if (LentCookers.FindOrAdd(TextureSet) > 0)
				CanDelete = false;

			if (CanDelete)
			{
				CookersToDelete.Add(TextureSet);
			}
		}
	}

	for (UTextureSet* TextureSet : CookersToDelete)
	{
		Cookers.Remove(TextureSet);
		PostCompilation(TextureSet);
	}

	if (QueuedTextureSets.Num() > 0 && Cookers.Num() < CVarMaxAsyncTextureSetCooks.GetValueOnGameThread())
	{
		TArray<UTextureSet*> TextureSetsToStart;
		TextureSetsToStart.Reserve(QueuedTextureSets.Num());

		for (TSoftObjectPtr<UTextureSet> QueuedTextureSet : QueuedTextureSets)
		{
			if (!HasTimeLeft())
				break;

			UTextureSet* TextureSet = QueuedTextureSet.LoadSynchronous();

			// Texture Set may be null if deleted
			if (!IsValid(TextureSet))
				continue;
			
			if (!CompilingTextureSets.Contains(TextureSet) || TryCancelCompilation(TextureSet))
			{
				// The texture set is not currently compiling, or was but the async job could be cancelled, so we are safe to kick it off.
				TextureSetsToStart.Add(TextureSet);
			}
		}

		// Remove all elements that we have started compilation of from the queued array
		for (UTextureSet* TextureSet : TextureSetsToStart)
		{
			// StartCompilation will remove the texture set from the queue
			StartCompilation(TextureSet);
		}
	}
}

int32 FTextureSetCompilingManager::GetNumRemainingAssets() const
{
	check(IsInGameThread());
	return Cookers.Num();
}

void FTextureSetCompilingManager::ProcessAsyncTasks(bool bLimitExecutionTime)
{
	FObjectCacheContextScope ObjectCacheScope;

	ProcessTextureSets(bLimitExecutionTime);

	UpdateCompilationNotification();
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
