// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "AsyncCompilationHelpers.h"
#include "Containers/Set.h"
#include "CoreMinimal.h"
#include "TextureSetCompiler.h"
#include "TextureSetCompilerTask.h"
#include "UObject/WeakObjectPtr.h"

class UTextureSet;
class FQueuedThreadPool;
enum class EQueuedWorkPriority : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FTextureSetPostCompileEvent, const TArrayView<UTextureSet* const>&);

class TEXTURESETS_API FTextureSetCompilingManager : IAssetCompilingManager
{
public:
	static FTextureSetCompilingManager& Get();

	void QueueCompilation(UTextureSet* const InTextureSet);

	// Adds textures compiled asynchronously
	void StartCompilation(UTextureSet* const InTextureSet, bool bAsync = true);

	// Blocks until completion of the requested textures.
	void FinishCompilation(TArrayView<UTextureSet* const> InTextureSets);

	// Try canceling the compilation of an async job. Returns true if the job was cancelled or no job existed in the first place.
	bool TryCancelCompilation(UTextureSet* const TextureSet);

	bool IsCompiling(const UTextureSet* TextureSet) const;
	bool IsQueued(const UTextureSet* TextureSet) const;
	bool IsRegistered(const UTextureSet* TextureSet) const;

	// Blocks until completion of all async texture set compilation.
	void FinishAllCompilation() override;

	// Returns if asynchronous compilation is allowed for this texture set.
	bool IsAsyncCompilationAllowed() const;

	// Returns the threadpool where texture compilation should be scheduled.
	FQueuedThreadPool* GetThreadPool() const;

	// Cancel any pending work and blocks until it is safe to shut down.
	void Shutdown() override;

	// Get the name of the asset type this compiler handles
	static FName GetStaticAssetTypeName();

	FTextureSetPostCompileEvent& OnTextureSetPostCompileEvent() { return TextureSetPostCompileEvent; }

	void NotifyMaterialInstances(TArrayView<UTextureSet* const> InTextureSets);

private:
	friend class FAssetCompilingManager;

	FTextureSetCompilingManager();

	FName GetAssetTypeName() const override;
	FTextFormat GetAssetNameFormat() const override;
	TArrayView<FName> GetDependentTypeNames() const override;
	int32 GetNumRemainingAssets() const override;
	void ProcessAsyncTasks(bool bLimitExecutionTime = false) override;

	void ProcessTextureSets(bool bLimitExecutionTime);
	void AssignDerivedData(UTextureSetDerivedData* NewDerivedData, UTextureSet* TextureSet);
	bool AllDependenciesLoaded(UMaterialInstance* MaterialInstance);
	void RefreshMaterialInstances();
	void UpdateCompilationNotification();

	TSharedRef<FTextureSetCompilerArgs> MakeCompilerArgs(UTextureSet* TextureSet);

	double LastReschedule = 0.0f;
	bool bHasShutdown = false;

	TSet<TSoftObjectPtr<UTextureSet>> QueuedTextureSets;
	TMap<UTextureSet*, TSharedPtr<TextureSetCompilerTask>> AsyncCompilationTasks;
	//auto& [TextureSet, Task]
	FAsyncCompilationNotification Notification;
	TSet<const UTextureSet*> MaterialInstancesToUpdate;

	/** Event issued at the end of the compile process */
	FTextureSetPostCompileEvent TextureSetPostCompileEvent;
};

#endif