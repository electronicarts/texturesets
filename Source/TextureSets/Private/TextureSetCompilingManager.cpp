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

		for (auto& [TextureSet, Cooker] : CompilingTextureSets)
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

void FTextureSetCompilingManager::QueueCompilation(TArrayView<UTextureSet* const> InTextureSets)
{
	for (UTextureSet* TextureSet : InTextureSets)
	{
		QueuedTextureSets.AddUnique(TextureSet);
	}
}

void FTextureSetCompilingManager::StartCompilation(TArrayView<UTextureSet* const> InTextureSets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::AddTextures)
	check(IsInGameThread());

	for (UTextureSet* TextureSet : InTextureSets)
	{
		checkf(!CompilingTextureSets.Contains(TextureSet), TEXT("Texture Set is already compiling so cannot be started. If you are unsure, call IsRegistered(), TryCancelCompilation() and/or FinishCompilation() before attempting to queue a texture set."));

		// Cancel or finish previous compilation
		if (IsRegistered(TextureSet) && !TryCancelCompilation(TextureSet))
		{
			FinishCompilation({TextureSet});
		}

		check(TextureSet->DerivedData.bIsCooking == false);

		TSharedRef<FTextureSetCooker> Cooker = MakeShared<FTextureSetCooker>(TextureSet, TextureSet->DerivedData);

		if (!Cooker->CookRequired())
		{
			return;
		}

		if (IsAsyncCompilationAllowed(TextureSet))
		{
			CompilingTextureSets.Emplace(TextureSet, Cooker);
			Cooker->ExecuteAsync();

			// Will swap out material instances with default values until cooking has finished
			TextureSet->NotifyMaterialInstances();
		}
		else
		{
			Cooker->Execute();
			PostCompilation(TextureSet, Cooker);
		}
	}

	TRACE_COUNTER_SET(QueuedTextureSetCompilation, GetNumRemainingAssets());
}

void FTextureSetCompilingManager::FinishCompilation(TArrayView<UTextureSet* const> InTextureSets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::FinishCompilation);

	check(IsInGameThread());

	TMap<TWeakObjectPtr<UTextureSet>, TSharedRef<FTextureSetCooker>> PendingTextureSets;
	PendingTextureSets.Reserve(InTextureSets.Num());

	int32 TextureIndex = 0;
	for (UTextureSet* TextureSet : InTextureSets)
	{
		if (CompilingTextureSets.Contains(TextureSet))
		{
			PendingTextureSets.Add(TextureSet, CompilingTextureSets.FindChecked(TextureSet));
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

			void Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority) final
			{
				if (FAsyncTaskBase* AsyncTask = GetAsyncTask())
					AsyncTask->SetPriority(InPriority);
			}

			bool WaitCompletionWithTimeout(float TimeLimitSeconds) final
			{
				if (FAsyncTaskBase* AsyncTask = GetAsyncTask())
					return AsyncTask->WaitCompletionWithTimeout(TimeLimitSeconds);
				else
					return true;
			}

			FName GetName() override { return Cooker->GetTextureSetName(); }

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
				PostCompilation(TextureSet, Compilable->Cooker);
				CompilingTextureSets.Remove(TextureSet);
			}
		);
	}
}

bool FTextureSetCompilingManager::TryCancelCompilation(UTextureSet* const TextureSet)
{
	if (!CompilingTextureSets.Contains(TextureSet))
		return true;

	TSharedRef<FTextureSetCooker> Cooker = CompilingTextureSets.FindChecked(TextureSet);
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
	return CompilingTextureSets.Contains(TextureSet) || QueuedTextureSets.Contains(TextureSet);
}

void FTextureSetCompilingManager::PostCompilation(UTextureSet* TextureSet, TSharedRef<FTextureSetCooker> Cooker)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::PostCompilation);

	Cooker->Finalize();

	// Update loaded material instances that use this texture set
	TextureSet->NotifyMaterialInstances();

	UE_LOG(LogTexture, Verbose, TEXT("Texture set %s is ready"), *TextureSet->GetName());
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

		for (auto& [TextureSet, Cooker] : CompilingTextureSets)
		{
			check(IsValid(TextureSet));
			PendingTextureSets.Add(TextureSet);
		}

		FinishCompilation(PendingTextureSets);
	}
}

void FTextureSetCompilingManager::ProcessTextureSets(bool bLimitExecutionTime)
{
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

			TMap<UTextureSet*, TSharedRef<FTextureSetCooker>> TextureSetsToPostpone;
			for (auto& [TextureSet, Cooker] : CompilingTextureSets)
			{
				check(IsValid(TextureSet));

				// HasTimeLeft() ensures we don't stall the editor if too many texture sets finish at the same time
				if (HasTimeLeft() && !Cooker->IsAsyncJobInProgress())
				{
					PostCompilation(TextureSet, Cooker);
				}
				else
				{
					TextureSetsToPostpone.Emplace(MoveTemp(TextureSet), Cooker);
				}
			}

			CompilingTextureSets = MoveTemp(TextureSetsToPostpone);
		}
	}

	if (QueuedTextureSets.Num() > 0)
	{
		TArray<TSoftObjectPtr<UTextureSet>> PendingTextureSets;
		PendingTextureSets.Reserve(CompilingTextureSets.Num());

		int i = 0;
		for (; i < QueuedTextureSets.Num() && HasTimeLeft(); i++)
		{
			UTextureSet* TextureSet = QueuedTextureSets[i].LoadSynchronous();

			// Texture Set may be null if deleted
			if (!IsValid(TextureSet))
				continue;
			
			if (!CompilingTextureSets.Contains(TextureSet) || TryCancelCompilation(TextureSet))
			{
				// The texture set is not currently compiling, or was but the async job could be cancelled, so we are safe to kick it off.
				StartCompilation({TextureSet});
			}
			else
			{
				// Texture set is currently compiling, and was queued again.
				// Keep it in queue so when the current compilation does finish, it will run again with the latest data.
				PendingTextureSets.Add(QueuedTextureSets[i]);
			}
		}

		// Remove all elements that we have started compilation of from the queued array
		for (int j = i; j < QueuedTextureSets.Num(); j++)
		{
			QueuedTextureSets[j - i] = QueuedTextureSets[j];
		}

		QueuedTextureSets.SetNum(QueuedTextureSets.Num() - i);
	}
}

int32 FTextureSetCompilingManager::GetNumRemainingAssets() const
{
	return CompilingTextureSets.Num();
}

void FTextureSetCompilingManager::ProcessAsyncTasks(bool bLimitExecutionTime)
{
	FObjectCacheContextScope ObjectCacheScope;

	ProcessTextureSets(bLimitExecutionTime);

	UpdateCompilationNotification();
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
