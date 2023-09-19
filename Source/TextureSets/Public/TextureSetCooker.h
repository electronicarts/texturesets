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

class TextureSetDerivedDataPlugin : public FDerivedDataPluginInterface
{
public:
	TextureSetDerivedDataPlugin(TextureSetCooker* Cooker);

	// FDerivedDataPluginInterface
	virtual const TCHAR* GetPluginName() const override;
	virtual const TCHAR* GetVersionString() const override;
	virtual FString GetPluginSpecificCacheKeySuffix() const override;
	virtual bool IsBuildThreadsafe() const override;
	virtual bool IsDeterministic() const override;
	virtual FString GetDebugContextString() const override;
	virtual bool Build(TArray<uint8>& OutData) override;

private:
	TextureSetCooker* Cooker;
};

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
	friend class TextureSetDerivedDataPlugin;
	friend class FTextureSetCookerTaskWorker;
public:

	TextureSetCooker(UTextureSet* TS);

	bool IsOutOfDate() const;
	bool IsOutOfDate(int PackedTextureIndex) const;

	void ConfigureTexture(int Index) const;

	void Execute();
	void ExecuteAsync(FQueuedThreadPool* InQueuedPool = GThreadPool, EQueuedWorkPriority InQueuedWorkPriority = EQueuedWorkPriority::Normal);

	bool IsAsyncJobInProgress();
	bool TryCancel();

	FAsyncTaskBase* GetAsyncTask() { return AsyncTask.Get(); }

private:
	UTextureSet* TextureSet;

	FTextureSetProcessingContext Context;
	FTextureSetProcessingGraph Graph;
	
	const FTextureSetDefinitionModuleInfo ModuleInfo;
	const FTextureSetPackingInfo PackingInfo;

	FGuid TextureSetDataId;
	TArray<FGuid> PackedTextureIds;

	TUniquePtr<FAsyncTask<FTextureSetCookerTaskWorker>> AsyncTask;

	void ExecuteInternal();

	void Build() const;
	// Called for each packed texture of a texture set, can execute in parallel.
	void BuildTextureData(int Index) const;

	static inline int GetPixelIndex(int X, int Y, int Channel, int Width, int Height)
	{
		return Y * Width * 4
			+ X * 4
			+ Channel;
	}

};
#endif
