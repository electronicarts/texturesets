// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetCompilingManager.h"

#if WITH_EDITOR

#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetCooker.h"
#include "TextureSetModule.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/TextureDefines.h"
#include "Serialization/MemoryReader.h"
#include "ImageUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "ProfilingDebugging/CookStats.h"
#include "DerivedDataCacheInterface.h"
#include "Misc/DataValidation.h"
#include "DerivedDataBuildVersion.h"
#include "AsyncCompilationHelpers.h"
#include "AssetCompilingManager.h"
#include "Misc/QueuedThreadPoolWrapper.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ObjectCacheContext.h"

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


bool FTextureSetCompilingManager::IsCompilingTextureSet(UTextureSet* InTextureSet) const
{
	check(IsInGameThread());

	if (!InTextureSet)
	{
		return false;
	}

	const TWeakObjectPtr<UTextureSet> InWeakTextureSetPtr = InTextureSet;
	uint32 Hash = GetTypeHash(InWeakTextureSetPtr);
	for (const TSet<TWeakObjectPtr<UTextureSet>>& Bucket : RegisteredTextureSetBuckets)
	{
		if (Bucket.ContainsByHash(Hash, InWeakTextureSetPtr))
		{
			return true;
		}
	}

	return false;
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
	if (GetNumRemainingTextureSets())
	{
		TArray<UTextureSet*> PendingTextureSets;
		PendingTextureSets.Reserve(GetNumRemainingTextureSets());

		for (TSet<TWeakObjectPtr<UTextureSet>>& Bucket : RegisteredTextureSetBuckets)
		{
			for (TWeakObjectPtr<UTextureSet>& WeakTexture : Bucket)
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
	TRACE_COUNTER_SET(QueuedTextureSetCompilation, GetNumRemainingTextureSets());
	Notification.Update(GetNumRemainingTextureSets());
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

int32 FTextureSetCompilingManager::GetNumRemainingTextureSets() const
{
	int32 Num = 0;
	for (const TSet<TWeakObjectPtr<UTextureSet>>& Bucket : RegisteredTextureSetBuckets)
	{
		Num += Bucket.Num();
	}

	return Num;
}

int32 FTextureSetCompilingManager::GetNumRemainingAssets() const
{
	return GetNumRemainingTextureSets();
}

void FTextureSetCompilingManager::AddTextureSets(TArrayView<UTextureSet* const> InTextureSets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::AddTextures)
	check(IsInGameThread());

	for (UTextureSet* TextureSet : InTextureSets)
	{
		int32 TexturePriority = 0;

		if (RegisteredTextureSetBuckets.Num() <= TexturePriority)
		{
			RegisteredTextureSetBuckets.SetNum(TexturePriority + 1);
		}
		RegisteredTextureSetBuckets[TexturePriority].Emplace(TextureSet);
	}

	TRACE_COUNTER_SET(QueuedTextureSetCompilation, GetNumRemainingTextureSets());
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
		for (TSet<TWeakObjectPtr<UTextureSet>>& Bucket : RegisteredTextureSetBuckets)
		{
			if (Bucket.Contains(TextureSet))
			{
				PendingTextureSets.Add(TextureSet);
			}
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

			void Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority) final
			{

			}

			bool WaitCompletionWithTimeout(float TimeLimitSeconds) final
			{
				return false;
			}

			FName GetName() override { return TextureSet->GetOutermost()->GetFName(); }

			TStrongObjectPtr<UTextureSet> TextureSet;
		};

		TArray<UTextureSet*> UniqueTextures(PendingTextureSets.Array());
		TArray<FCompilableTextureSet> CompilableTextures(UniqueTextures);

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

				for (TSet<TWeakObjectPtr<UTextureSet>>& Bucket : RegisteredTextureSetBuckets)
				{
					Bucket.Remove(TextureSet);
				}
			}
		);

		PostCompilation(UniqueTextures);
	}
}

void FTextureSetCompilingManager::PostCompilation(UTextureSet* TextureSet)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::PostCompilation);

	TextureSet->OnFinishCook();

	UE_LOG(LogTexture, Verbose, TEXT("Texture set %s is ready"), *TextureSet->GetName());
}

void FTextureSetCompilingManager::PostCompilation(TArrayView<UTextureSet* const> InCompiledTextures)
{
	for (UTextureSet* TextureSet : InCompiledTextures)
	{
		PostCompilation(TextureSet);
	}
}

void FTextureSetCompilingManager::FinishAllCompilation()
{
	UE_SCOPED_ENGINE_ACTIVITY(TEXT("Finish TextureSet Compilation"));

	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::FinishAllCompilation)

	if (GetNumRemainingTextureSets())
	{
		TArray<UTextureSet*> PendingTextureSets;
		PendingTextureSets.Reserve(GetNumRemainingTextureSets());

		for (TSet<TWeakObjectPtr<UTextureSet>>& Bucket : RegisteredTextureSetBuckets)
		{
			for (TWeakObjectPtr<UTextureSet>& TextureSet : Bucket)
			{
				if (TextureSet.IsValid())
				{
					PendingTextureSets.Add(TextureSet.Get());
				}
			}
		}

		FinishCompilation(PendingTextureSets);
	}
}

bool FTextureSetCompilingManager::RequestPriorityChange(UTextureSet* InTexture, EQueuedWorkPriority InPriority)
{
	return false;
}

void FTextureSetCompilingManager::ProcessTextureSets(bool bLimitExecutionTime, int32 MaximumPriority)
{
	using namespace TextureSetCompilingManagerImpl;
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureSetCompilingManager::ProcessTextures);
	const double MaxSecondsPerFrame = 0.016;

	if (GetNumRemainingTextureSets())
	{
		FObjectCacheContextScope ObjectCacheScope;
		TArray<UTextureSet*> ProcessedTextures;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ProcessFinishedTextures);

			double TickStartTime = FPlatformTime::Seconds();

			if (MaximumPriority == -1 || MaximumPriority > RegisteredTextureSetBuckets.Num())
			{
				MaximumPriority = RegisteredTextureSetBuckets.Num();
			}

			for (int32 PriorityIndex = 0; PriorityIndex < MaximumPriority; ++PriorityIndex)
			{
				TSet<TWeakObjectPtr<UTextureSet>>& TextureSetsToProcess = RegisteredTextureSetBuckets[PriorityIndex];
				if (TextureSetsToProcess.Num())
				{
					const bool bIsHighestPrio = PriorityIndex == 0;

					TSet<TWeakObjectPtr<UTextureSet>> TextureSetsToPostpone;
					for (TWeakObjectPtr<UTextureSet>& TextureSet : TextureSetsToProcess)
					{
						if (TextureSet.IsValid())
						{
							const bool bHasTimeLeft = bLimitExecutionTime ? ((FPlatformTime::Seconds() - TickStartTime) < MaxSecondsPerFrame) : true;
							if ((bIsHighestPrio || bHasTimeLeft) && TextureSet->IsAsyncCookComplete())
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

					RegisteredTextureSetBuckets[PriorityIndex] = MoveTemp(TextureSetsToPostpone);
				}
			}
		}

		PostCompilation(ProcessedTextures);
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
