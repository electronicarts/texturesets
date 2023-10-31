// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "DerivedDataPluginInterface.h"
#include "TextureSetDerivedData.h"
#include "TextureSetInfo.h"
#include "TextureSetModule.h"
#include "TextureSetProcessingGraph.h"
#include "TextureSetProcessingContext.h"

class UTextureSet;
class FTextureSetCooker;
class ITextureProcessingNode;

class FTextureSetCookerTaskWorker : public FNonAbandonableTask
{
public:
	FTextureSetCookerTaskWorker (FTextureSetCooker* Cooker)
		: Cooker(Cooker) {}

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FTextureSetCookerTaskWorker, STATGROUP_ThreadPoolAsyncTasks); }
	void DoWork();

private:
	FTextureSetCooker* Cooker;
};

/**
* The Texture Set Cooker generates derived data for a given texture set.
* It copies everything it needs from the texture set and the definition in
* the constructor, so changes to the data won't affect a cook in progress.
* 
* Intended usage looks something like:
* 
*	FTextureSetCooker Cooker(TextureSet, DerivedData);
*	if (Cooker.CookRequired())
*	{
*		Cooker.Execute();
*		Cooker.Finalize();
*	}
* 
* The cooker also supports async execution, which can check for completion with IsAsyncJobInProgress()
*/
class FTextureSetCooker
{
	friend class TextureSetDerivedTextureDataPlugin;
	friend class TextureSetDerivedParameterDataPlugin;
	friend class FTextureSetCookerTaskWorker;
public:

	FTextureSetCooker(UTextureSet* TextureSet, FTextureSetDerivedData& DerivedData, TArray<const ITargetPlatform*> TargetPlatforms);

	bool CookRequired() const;

	void Execute();
	void ExecuteAsync(FQueuedThreadPool* InQueuedPool = GThreadPool, EQueuedWorkPriority InQueuedWorkPriority = EQueuedWorkPriority::Normal);

	void Finalize();

	bool IsAsyncJobInProgress() const;
	bool TryCancel();

	FAsyncTaskBase* GetAsyncTask() { return AsyncTask.Get(); }
	FName GetTextureSetName() { return FName(TextureSetName); }

	const TArray<const ITargetPlatform*>& GetPlatforms() { return TargetPlatforms; }

private:
	FTextureSetDerivedData& DerivedData;
	TArray<const ITargetPlatform*> TargetPlatforms;

	FTextureSetProcessingContext Context;
	TSharedPtr<FTextureSetProcessingGraph> GraphInstance;
	
	const FTextureSetPackingInfo PackingInfo;
	const FTextureSetDefinitionModuleInfo ModuleInfo;

	UObject* OuterObject;
	FString TextureSetName;
	FString TextureSetFullName;
	FString UserKey;
	TArray<FGuid> DerivedTextureIds;
	TMap<FName, FGuid> ParameterIds;

	TUniquePtr<FAsyncTask<FTextureSetCookerTaskWorker>> AsyncTask;

	// Compute the hashed Guid for a specific hashed texture. Used by the DDC to cache the data.
	FGuid ComputeTextureDataId(int Index, const TMap<FName, TSharedRef<ITextureProcessingNode>>& ProcessedTextures) const;
	FGuid ComputeParameterDataId(const IParameterProcessingNode* Parameter) const;

	void Prepare();

	void ConfigureTexture(int Index) const;

	void ExecuteInternal();

	FDerivedTextureData BuildTextureData(int Index) const;

	static inline int GetPixelIndex(int X, int Y, int Z, int Channel, int Width, int Height)
	{
		return Z * Width * Height * 4
			+ Y * Width * 4
			+ X * 4
			+ Channel;
	}

};
#endif
