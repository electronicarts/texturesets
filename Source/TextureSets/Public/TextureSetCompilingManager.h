// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Containers/Set.h"
#include "AssetCompilingManager.h"
#include "AsyncCompilationHelpers.h"

#if WITH_EDITOR

class UTextureSet;
class FQueuedThreadPool;
enum class EQueuedWorkPriority : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FTextureSetPostCompileEvent, const TArrayView<UTextureSet* const>&);

class TEXTURESETS_API FTextureSetCompilingManager : IAssetCompilingManager
{
public:
	static FTextureSetCompilingManager& Get();

	/**
	* Returns true if the feature is currently activated.
	*/
	bool IsAsyncTextureSetCompilationEnabled() const;

	/** 
	* Returns the number of outstanding texture compilations.
	*/
	int32 GetNumRemainingTextureSets() const;

	/** 
	* Adds textures compiled asynchronously so they are monitored. 
	*/
	void AddTextureSets(TArrayView<UTextureSet* const> InTextures);

	/** 
	* Blocks until completion of the requested textures.
	*/
	void FinishCompilation(TArrayView<UTextureSet* const> InTextures);

	/** 
	* Blocks until completion of all async texture set compilation.
	*/
	void FinishAllCompilation() override;

	/**
	* Returns if asynchronous compilation is allowed for this texture set.
	*/
	bool IsAsyncCompilationAllowed(UTextureSet* InTexture) const;

	/**
	* Request that the texture be processed at the specified priority.
	*/
	bool RequestPriorityChange(UTextureSet* InTexture, EQueuedWorkPriority Priority);

	/**
	* Returns the threadpool where texture compilation should be scheduled.
	*/
	FQueuedThreadPool* GetThreadPool() const;

	/**
	* Cancel any pending work and blocks until it is safe to shut down.
	*/
	void Shutdown() override;

	/** Get the name of the asset type this compiler handles */
	static FName GetStaticAssetTypeName();

	/** Return true if the texture is currently compiled */
	bool IsCompilingTextureSet(UTextureSet* InTexture) const;

	FTextureSetPostCompileEvent& OnTextureSetPostCompileEvent() { return TextureSetPostCompileEvent; }

private:
	friend class FAssetCompilingManager;
	friend class UTextureSet;

	FTextureSetCompilingManager();

	FName GetAssetTypeName() const override;
	FTextFormat GetAssetNameFormat() const override;
	TArrayView<FName> GetDependentTypeNames() const override;
	int32 GetNumRemainingAssets() const override;
	void ProcessAsyncTasks(bool bLimitExecutionTime = false) override;

	void ProcessTextureSets(bool bLimitExecutionTime, int32 MaximumPriority = -1);
	void UpdateCompilationNotification();

	void PostCompilation(UTextureSet* TextureSet);
	void PostCompilation(TArrayView<UTextureSet* const> InCompiledTextureSets);

	double LastReschedule = 0.0f;
	bool bHasShutdown = false;
	TArray<TSet<TWeakObjectPtr<UTextureSet>>> RegisteredTextureSetBuckets;
	FAsyncCompilationNotification Notification;

	/** Event issued at the end of the compile process */
	FTextureSetPostCompileEvent TextureSetPostCompileEvent;
};

#endif