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

void FTextureSetCompilingManager::QueueCompilation(UTextureSet* const InTextureSet, const ITargetPlatform* TargetPlatform)
{
	TArray<const ITargetPlatform*>& Platforms = QueuedTextureSets.FindOrAdd(InTextureSet);
	Platforms.AddUnique(TargetPlatform);
}

void FTextureSetCompilingManager::StartCompilation(UTextureSet* const TextureSet, const TArray<const ITargetPlatform*>& TargetPlatforms)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::StartCompilation)
	check(IsInGameThread());

	checkf(!CompilingTextureSets.Contains(TextureSet), TEXT("Texture Set is already compiling so cannot be started. If you are unsure, call IsRegistered(), TryCancelCompilation() and/or FinishCompilation() before attempting to queue a texture set."));

	// Cancel or finish previous compilation
	if (IsRegistered(TextureSet) && !TryCancelCompilation(TextureSet))
	{
		FinishCompilation({TextureSet});
	}

	check(TextureSet->DerivedData.bIsCooking == false);

	TSharedRef<FTextureSetCooker> Cooker = MakeShared<FTextureSetCooker>(TextureSet, TextureSet->DerivedData, TargetPlatforms);

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
		Cooker->Finalize();
		PostCompilation(TextureSet, Cooker);
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

				Compilable->Cooker->Finalize();
				UTextureSet* TextureSet = Compilable->TextureSet.Get();
				CompilingTextureSets.Remove(TextureSet);
				PostCompilation(TextureSet, Compilable->Cooker);
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
	check(IsInGameThread());
	return CompilingTextureSets.Contains(TextureSet) || QueuedTextureSets.Contains(TextureSet);
}

void FTextureSetCompilingManager::PostCompilation(UTextureSet* TextureSet, TSharedRef<FTextureSetCooker> Cooker)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::PostCompilation);

	// If platforms contains a null entry, it means we're targeting the current platform and we want to see our results right away
	if (Cooker->GetPlatforms().Contains(nullptr))
	{
		// Update loaded material instances that use this texture set
		TextureSet->NotifyMaterialInstances();
	}

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

			TMap<UTextureSet*, TSharedRef<FTextureSetCooker>> FinishedTextureSets;
			TMap<UTextureSet*, TSharedRef<FTextureSetCooker>> TextureSetsToPostpone;
			for (auto& [TextureSet, Cooker] : CompilingTextureSets)
			{
				check(IsValid(TextureSet));

				// HasTimeLeft() ensures we don't stall the editor if too many texture sets finish at the same time
				if (HasTimeLeft() && !Cooker->IsAsyncJobInProgress())
				{
					Cooker->Finalize();
					FinishedTextureSets.Emplace(MoveTemp(TextureSet), Cooker);
				}
				else
				{
					TextureSetsToPostpone.Emplace(MoveTemp(TextureSet), Cooker);
				}
			}

			CompilingTextureSets = MoveTemp(TextureSetsToPostpone);

			// Important that PostCompilation is called after CompilingTextureSets so the texture sets return false in IsCompiling()
			for (auto& [TextureSet, Cooker] : FinishedTextureSets)
			{
				PostCompilation(TextureSet, Cooker);
			}
		}
	}

	if (QueuedTextureSets.Num() > 0)
	{
		TArray<UTextureSet*> StartedTextureSets;
		StartedTextureSets.Reserve(QueuedTextureSets.Num());

		for (auto& [QueuedTextureSet, Platforms] : QueuedTextureSets)
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
				StartCompilation(TextureSet, Platforms);
				StartedTextureSets.Add(TextureSet);
			}
		}

		// Remove all elements that we have started compilation of from the queued array
		for (UTextureSet* Started : StartedTextureSets)
		{
			QueuedTextureSets.Remove(Started);
		}
	}
}

int32 FTextureSetCompilingManager::GetNumRemainingAssets() const
{
	check(IsInGameThread());
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
