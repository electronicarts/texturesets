// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "DerivedDataPluginInterface.h"
#include "TextureSetDerivedData.h"
#include "TextureSetInfo.h"
#include "TextureSetModule.h"
#include "TextureSetProcessingGraph.h"

class UTextureSet;
class TextureSetCooker;

class FTextureSetCookerTaskWorker : public FNonAbandonableTask
{
public:
	FTextureSetCookerTaskWorker (TextureSetCooker* Cooker)
		: Cooker(Cooker)
	{}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTextureSetCookerTaskWorker, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork();

private:
	TextureSetCooker* Cooker;
};

class TextureSetCooker
{
	friend class TextureSetDerivedTextureDataPlugin;
	friend class TextureSetDerivedParameterDataPlugin;
	friend class FTextureSetCookerTaskWorker;
public:

	TextureSetCooker(UTextureSet* TS);

	bool CookRequired() const;

	void Execute();
	void ExecuteAsync(FQueuedThreadPool* InQueuedPool = GThreadPool, EQueuedWorkPriority InQueuedWorkPriority = EQueuedWorkPriority::Normal);

	void Finalize();

	bool IsAsyncJobInProgress() const;
	bool TryCancel();

	FAsyncTaskBase* GetAsyncTask() { return AsyncTask.Get(); }

private:
	UTextureSetDefinition* Definition;
	FTextureSetDerivedData& DerivedData;

	FTextureSetProcessingContext Context;
	FTextureSetProcessingGraph Graph;
	
	const FTextureSetDefinitionModuleInfo ModuleInfo;
	const FTextureSetPackingInfo PackingInfo;

	UObject* OuterObject;
	FString TextureSetName;
	FString TextureSetFullName;
	FString UserKey;
	TArray<FGuid> DerivedTextureIds;
	TMap<FName, FGuid> ParameterIds;

	TUniquePtr<FAsyncTask<FTextureSetCookerTaskWorker>> AsyncTask;

	// Compute the hashed Guid for a specific hashed texture. Used by the DDC to cache the data.
	FGuid ComputeTextureDataId(int Index) const;
	FGuid ComputeParameterDataId(FName ParameterName) const;

	void Prepare();

	void ConfigureTexture(int Index) const;

	void ExecuteInternal();

	FDerivedTextureData BuildTextureData(int Index) const;

	static inline int GetPixelIndex(int X, int Y, int Channel, int Width, int Height)
	{
		return Y * Width * 4
			+ X * 4
			+ Channel;
	}

};
#endif
