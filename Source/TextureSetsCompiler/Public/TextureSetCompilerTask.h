// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FTextureSetCompiler;
class UTextureSetDerivedData;

class TEXTURESETSCOMPILER_API FTextureSetCompilerTaskWorker : public FNonAbandonableTask
{
public:
	FTextureSetCompilerTaskWorker (TSharedRef<FTextureSetCompiler> Compiler, UTextureSetDerivedData* DerivedData, bool bIsDefaultTextureSet);

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FTextureSetCompilerTaskWorker, STATGROUP_ThreadPoolAsyncTasks); }
	void DoWork();

private:
	TSharedRef<FTextureSetCompiler> Compiler;
	TStrongObjectPtr<UTextureSetDerivedData> DerivedData;
	bool bIsDefaultTextureSet;
};

class TEXTURESETSCOMPILER_API TextureSetCompilerTask
{
public:
	TextureSetCompilerTask(TSharedRef<FTextureSetCompiler> Compiler, bool bIsDefaultTextureSet);

	void Start();
	void StartAsync(FQueuedThreadPool* InQueuedPool, EQueuedWorkPriority InQueuedWorkPriority);

	bool TryFinalize();
	void Finalize();

	void SetPriority(EQueuedWorkPriority InPriority) { AsyncTask->SetPriority(InPriority); }
	bool Cancel() { return AsyncTask->Cancel(); }

	TSharedRef<FTextureSetCompiler> GetCompiler() const { return Compiler; }
	TObjectPtr<UTextureSetDerivedData> GetDerivedData() { check(bHasFinalized); return DerivedData.Get(); }

private:
	void CreateDerivedData();

	const TSharedRef<FTextureSetCompiler> Compiler;
	TStrongObjectPtr<UTextureSetDerivedData> DerivedData;
	bool bIsDefaultTextureSet;
	bool bHasBeganTextureCache;
	bool bHasAddedSourceProviders;
	bool bHasFinalized;

	TUniquePtr<FAsyncTask<FTextureSetCompilerTaskWorker>> AsyncTask;
};
