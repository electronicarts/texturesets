// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "AssetCompilingManager.h"
#include "AsyncCompilationHelpers.h"
#include "Containers/Set.h"
#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

#if WITH_EDITOR

class UTextureSet;
class FQueuedThreadPool;
class FTextureSetCooker;
enum class EQueuedWorkPriority : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FTextureSetPostCompileEvent, const TArrayView<UTextureSet* const>&);

class TEXTURESETS_API FTextureSetCompilingManager : IAssetCompilingManager
{
public:
	static FTextureSetCompilingManager& Get();

	// Adds textures compiled asynchronously
	void StartCompilation(TArrayView<UTextureSet* const> InTextureSets);

	// Blocks until completion of the requested textures.
	void FinishCompilation(TArrayView<UTextureSet* const> InTextureSets);

	bool TryCancelCompilation(UTextureSet* const TextureSet);

	bool IsRegistered(const UTextureSet* TextureSet) const;

	// Blocks until completion of all async texture set compilation.
	void FinishAllCompilation() override;

	// Returns if asynchronous compilation is allowed for this texture set.
	bool IsAsyncCompilationAllowed(UTextureSet* InTextureSets) const;

	// Returns the threadpool where texture compilation should be scheduled.
	FQueuedThreadPool* GetThreadPool() const;

	// Cancel any pending work and blocks until it is safe to shut down.
	void Shutdown() override;

	// Get the name of the asset type this compiler handles
	static FName GetStaticAssetTypeName();

	FTextureSetPostCompileEvent& OnTextureSetPostCompileEvent() { return TextureSetPostCompileEvent; }

private:
	friend class FAssetCompilingManager;

	FTextureSetCompilingManager();

	FName GetAssetTypeName() const override;
	FTextFormat GetAssetNameFormat() const override;
	TArrayView<FName> GetDependentTypeNames() const override;
	int32 GetNumRemainingAssets() const override;
	void ProcessAsyncTasks(bool bLimitExecutionTime = false) override;

	void ProcessTextureSets(bool bLimitExecutionTime);
	void UpdateCompilationNotification();

	void PostCompilation(UTextureSet* TextureSet, TSharedRef<FTextureSetCooker> Cooker);

	double LastReschedule = 0.0f;
	bool bHasShutdown = false;
	TMap<UTextureSet*, TSharedRef<FTextureSetCooker>> RegisteredTextureSets;
	FAsyncCompilationNotification Notification;

	/** Event issued at the end of the compile process */
	FTextureSetPostCompileEvent TextureSetPostCompileEvent;
};

#endif