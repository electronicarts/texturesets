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

DECLARE_LOG_CATEGORY_EXTERN(LogTextureSetCook, Log, All);

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
*		Cooker.Prepare();
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

	FTextureSetCooker(UTextureSet* TextureSet, FTextureSetDerivedData& DerivedData);

	bool CookRequired() const;

	void Prepare();

	void Execute();
	void ExecuteAsync(FQueuedThreadPool* InQueuedPool = GThreadPool, EQueuedWorkPriority InQueuedWorkPriority = EQueuedWorkPriority::Normal);

	bool IsAsyncJobInProgress() const;
	bool TryCancel();

	FAsyncTaskBase* GetAsyncTask() { return AsyncTask.Get(); }
	FName GetTextureSetName() { return FName(TextureSetName); }

	void ConfigureTexture(int Index) const;
	void InitializeTextureData(int Index) const;
	void BuildTextureData(int Index) const;

	const FTextureSetDerivedData& GetDerivedData() const { return DerivedData; }
	const FGuid GetDerivedTextureID(int Index) const { return DerivedTextureIds[Index]; }

private:
	FTextureSetDerivedData& DerivedData;

	FTextureSetProcessingContext Context;
	TSharedPtr<FTextureSetProcessingGraph> GraphInstance;
	
	const FTextureSetPackingInfo PackingInfo;
	const FTextureSetDefinitionModuleInfo ModuleInfo;

	UObject* OuterObject;
	FString TextureSetName;
	FString TextureSetFullName;
	FString UserKey;
	const bool bIsDefaultTextureSet;
	TArray<FGuid> DerivedTextureIds;
	TMap<FName, FGuid> ParameterIds;

	mutable TArray<FCriticalSection> TextureDataCS;

	enum class ETextureState
	{
		Unmodified,
		Configured,
		Initialized,
		Built
	};

	// Keeps track of which state textures are in so we avoid doing the same operations multiple times for a cooker
	mutable TArray<ETextureState> TextureStates;

	TUniquePtr<FAsyncTask<FTextureSetCookerTaskWorker>> AsyncTask;

	// Compute the hashed Guid for a specific hashed texture. Used by the DDC to cache the data.
	FGuid ComputeTextureDataId(int Index, const TMap<FName, TSharedRef<ITextureProcessingNode>>& ProcessedTextures) const;
	FGuid ComputeParameterDataId(const IParameterProcessingNode* Parameter) const;

	void ExecuteInternal();

	static inline int GetPixelIndex(int X, int Y, int Z, int Channel, int Width, int Height, int PixelStride)
	{
		return ((Z * Width * Height) + (Y * Width) + X) * PixelStride + Channel;
	}

};
#endif
