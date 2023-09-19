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
	if (RegisteredTextureSets.Num() > 0)
	{
		TArray<UTextureSet*> PendingTextureSets;
		PendingTextureSets.Reserve(RegisteredTextureSets.Num());

		for (TWeakObjectPtr<UTextureSet>& WeakTexture : RegisteredTextureSets)
		{
			if (WeakTexture.IsValid())
			{
				UTextureSet* TextureSet = WeakTexture.Get();

				if (!TextureSet->TryCancelCook())
				{
					PendingTextureSets.Add(TextureSet);
				}
			}
		}

		// Wait on texture sets already in progress we couldn't cancel
		FinishCompilation(PendingTextureSets);
	}
}

bool FTextureSetCompilingManager::IsAsyncTextureSetCompilationEnabled() const
{
	if (bHasShutdown || !FPlatformProcess::SupportsMultithreading())
		return false;
	else
		return CVarAsyncTextureSetStandard.AsyncCompilation.GetValueOnAnyThread() != 0;
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
	else
		return IsAsyncTextureSetCompilationEnabled();
}

FTextureSetCompilingManager& FTextureSetCompilingManager::Get()
{
	static FTextureSetCompilingManager Singleton;
	return Singleton;
}

int32 FTextureSetCompilingManager::GetNumRemainingAssets() const
{
	return RegisteredTextureSets.Num();
}

void FTextureSetCompilingManager::StartCompilation(TArrayView<UTextureSet* const> InTextureSets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::AddTextures)
	check(IsInGameThread());

	for (UTextureSet* TextureSet : InTextureSets)
	{
		check(TextureSet->ActiveCooker.IsValid());
		check(!TextureSet->ActiveCooker->IsAsyncJobInProgress());

		RegisteredTextureSets.Emplace(TextureSet);
		TextureSet->ActiveCooker->ExecuteAsync();
	}

	TRACE_COUNTER_SET(QueuedTextureSetCompilation, GetNumRemainingAssets());
}

void FTextureSetCompilingManager::FinishCompilation(TArrayView<UTextureSet* const> InTextureSets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::FinishCompilation);

	check(IsInGameThread());

	TSet<UTextureSet*> PendingTextureSets;
	PendingTextureSets.Reserve(InTextureSets.Num());

	int32 TextureIndex = 0;
	for (UTextureSet* TextureSet : InTextureSets)
	{
		if (RegisteredTextureSets.Contains(TextureSet))
		{
			PendingTextureSets.Add(TextureSet);
		}
	}

	if (PendingTextureSets.Num())
	{
		class FCompilableTextureSet final : public AsyncCompilationHelpers::ICompilable
		{
		public:
			FCompilableTextureSet(UTextureSet* InTexture)
				: TextureSet(InTexture)
			{
			}

			FAsyncTaskBase* GetAsyncTask()
			{
				if (TextureSet->ActiveCooker.IsValid())
					return TextureSet->ActiveCooker->GetAsyncTask();
				else
					return nullptr;
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

			FName GetName() override { return TextureSet->GetOutermost()->GetFName(); }

			TStrongObjectPtr<UTextureSet> TextureSet;
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
				UTextureSet* TextureSet = static_cast<FCompilableTextureSet*>(Object)->TextureSet.Get();
				PostCompilation(TextureSet);
				RegisteredTextureSets.Remove(TextureSet);
			}
		);
	}
}

void FTextureSetCompilingManager::PostCompilation(UTextureSet* TextureSet)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::PostCompilation);

	TextureSet->OnFinishCook();

	UE_LOG(LogTexture, Verbose, TEXT("Texture set %s is ready"), *TextureSet->GetName());
}

void FTextureSetCompilingManager::FinishAllCompilation()
{
	UE_SCOPED_ENGINE_ACTIVITY(TEXT("Finish TextureSet Compilation"));

	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::FinishAllCompilation)

	if (RegisteredTextureSets.Num() > 0)
	{
		TArray<UTextureSet*> PendingTextureSets;
		PendingTextureSets.Reserve(RegisteredTextureSets.Num());

		for (TWeakObjectPtr<UTextureSet>& TextureSet : RegisteredTextureSets)
		{
			if (TextureSet.IsValid())
			{
				PendingTextureSets.Add(TextureSet.Get());
			}
		}

		FinishCompilation(PendingTextureSets);
	}
}

void FTextureSetCompilingManager::ProcessTextureSets(bool bLimitExecutionTime)
{
	using namespace TextureSetCompilingManagerImpl;
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::ProcessTextures);
	const double MaxSecondsPerFrame = 0.016;

	if (RegisteredTextureSets.Num() > 0)
	{
		FObjectCacheContextScope ObjectCacheScope;
		TArray<UTextureSet*> ProcessedTextures;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ProcessFinishedTextures);

			double TickStartTime = FPlatformTime::Seconds();

			TSet<TWeakObjectPtr<UTextureSet>> TextureSetsToPostpone;
			for (TWeakObjectPtr<UTextureSet>& TextureSet : RegisteredTextureSets)
			{
				if (TextureSet.IsValid())
				{
					// Ensures we don't stall the editor if too many texture sets finish at the same time
					const bool bHasTimeLeft = bLimitExecutionTime ? ((FPlatformTime::Seconds() - TickStartTime) < MaxSecondsPerFrame) : true;

					if (bHasTimeLeft && TextureSet->IsAsyncCookComplete())
					{
						PostCompilation(TextureSet.Get());
						ProcessedTextures.Add(TextureSet.Get());
					}
					else
					{
						TextureSetsToPostpone.Emplace(MoveTemp(TextureSet));
					}
				}
			}

			RegisteredTextureSets = MoveTemp(TextureSetsToPostpone);
		}
	}
}

void FTextureSetCompilingManager::ProcessAsyncTasks(bool bLimitExecutionTime)
{
	FObjectCacheContextScope ObjectCacheScope;

	ProcessTextureSets(bLimitExecutionTime);

	UpdateCompilationNotification();
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
