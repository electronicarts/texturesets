// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetCompilerTask.h"

#include "DerivedDataBuildVersion.h"
#include "DerivedDataCacheInterface.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "TextureSetCompiler.h"
#include "TextureSetDerivedData.h"
#include "TextureSetTextureSourceProvider.h"

class TextureSetDerivedTextureDataPlugin : public FDerivedDataPluginInterface
{
public:
	TextureSetDerivedTextureDataPlugin(const FTextureSetCompiler& Compiler, FDerivedTexture& DerivedTexture, int32 DerivedTextureIndex)
		: Compiler(Compiler)
		, DerivedTexture(DerivedTexture)
		, DerivedTextureIndex(DerivedTextureIndex)
	{}

	// FDerivedDataPluginInterface
	virtual const TCHAR* GetPluginName() const override { return TEXT("TextureSet_FDerivedTextureData"); }
	virtual const TCHAR* GetVersionString() const override { return TEXT("F956B1B9-3AD3-47B3-BD82-8826C71A3DCB"); }
	virtual FString GetPluginSpecificCacheKeySuffix() const override { return Compiler.GetTextureDataId(DerivedTextureIndex).ToString(); }
	virtual bool IsBuildThreadsafe() const override { return false; }
	virtual bool IsDeterministic() const override { return true; }
	virtual FString GetDebugContextString() const override { return Compiler.Args->DebugContext; }
	
	virtual bool Build(TArray<uint8>& OutData) override
	{
		FTextureSource& Source = DerivedTexture.Texture->Source;

		if (DerivedTexture.TextureState < EDerivedTextureState::SourceInitialized)
			Compiler.InitializeTextureSource(DerivedTexture, DerivedTextureIndex);

		Compiler.GenerateTextureSource(DerivedTexture, DerivedTextureIndex);

		OutData.Empty(2048);
		FMemoryWriter DataWriter(OutData);
		DataWriter << DerivedTexture.Data;
		return true;
	}

private:
	const FTextureSetCompiler& Compiler;
	FDerivedTexture& DerivedTexture;
	const int32 DerivedTextureIndex;
};

class TextureSetDerivedParameterDataPlugin : public FDerivedDataPluginInterface
{
public:
	TextureSetDerivedParameterDataPlugin(const FTextureSetCompiler& Compiler, FName ParameterName)
		: Compiler(Compiler)
		, ParameterName(ParameterName)
	{}

	// FDerivedDataPluginInterface
	virtual const TCHAR* GetPluginName() const override { return TEXT("TextureSet_FDerivedTextureData"); }
	virtual const TCHAR* GetVersionString() const override { return TEXT("8D5EAAD9-8514-4957-B931-C7AF2698794F"); }
	virtual FString GetPluginSpecificCacheKeySuffix() const override { return Compiler.GetParameterDataId(ParameterName).ToString(); }
	virtual bool IsBuildThreadsafe() const override { return false; }
	virtual bool IsDeterministic() const override { return true; }
	virtual FString GetDebugContextString() const override { return Compiler.Args->DebugContext; }
	virtual bool Build(TArray<uint8>& OutData) override
	{
		FDerivedParameterData ParameterData = Compiler.BuildParameterData(ParameterName);

		OutData.Empty(sizeof(FDerivedParameterData));
		FMemoryWriter DataReader(OutData);
		DataReader << ParameterData;
		return true;
	}

private:
	const FTextureSetCompiler& Compiler;
	const FName ParameterName;
};

FTextureSetCompilerTaskWorker::FTextureSetCompilerTaskWorker (TSharedRef<FTextureSetCompiler> Compiler, UTextureSetDerivedData* DerivedData, bool bIsDefaultTextureSet)
	: Compiler(Compiler)
	, DerivedData(DerivedData)
	, bIsDefaultTextureSet(bIsDefaultTextureSet)
{}

void FTextureSetCompilerTaskWorker::DoWork()
{
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();

	ParallelForWithPreWork(
	Compiler->Args->PackingInfo.NumPackedTextures(),
	[&](int32 t) // Parallel For
	{
		// Retreive derived data from the DDC, or compute new data
		FDerivedTexture& DerivedTexture = DerivedData->Textures[t];
		FScopeLock Lock(DerivedTexture.TextureCS.Get());
		TArray<uint8> Data;
		if (DDC.GetSynchronous(new TextureSetDerivedTextureDataPlugin(Compiler.Get(), DerivedTexture, t), Data))
		{
			// De-serialized the data from the cache into the derived texture data
			FMemoryReader DataReader(Data);
			DataReader << DerivedTexture.Data;
		}

		// For default texture sets, we need to ensure they have valid source
		// data, since we will be not have a UTextureSetTextureSourceProvider.
		if (bIsDefaultTextureSet && DerivedTexture.TextureState < EDerivedTextureState::SourceGenerated)
		{
			Compiler->GenerateTextureSource(DerivedData->Textures[t], t);
		}
	},
	[&]() // Pre-Work
	{
		FScopeLock Lock(&DerivedData->ParameterCS);
		for (FName Name : Compiler->GetAllParameterNames())
		{
			FDerivedParameterData* OldParameterData = DerivedData->MaterialParameters.Find(Name);

			// Retreive derived data from the DDC, or compile new data
			TArray<uint8> Data;
			if (DDC.GetSynchronous(new TextureSetDerivedParameterDataPlugin(Compiler.Get(), Name), Data))
			{
				// De-serialized the data from the cache into the derived data
				FDerivedParameterData NewParameterData;
				FMemoryReader DataReader(Data);
				DataReader << NewParameterData;
				DerivedData->MaterialParameters.Emplace(Name, NewParameterData);
			}
		}
	});
}

TextureSetCompilerTask::TextureSetCompilerTask(TSharedRef<FTextureSetCompiler> Compiler, bool bIsDefaultTextureSet)
	: Compiler(Compiler)
	, DerivedData(nullptr)
	, bIsDefaultTextureSet(bIsDefaultTextureSet)
	, bHasBeganTextureCache(false)
	, bHasAddedSourceProviders(false)
	, bHasFinalized(false)
{
}

void TextureSetCompilerTask::Start()
{
	CreateDerivedData();
	AsyncTask = MakeUnique<FAsyncTask<FTextureSetCompilerTaskWorker>>(Compiler, DerivedData.Get(), bIsDefaultTextureSet);

	AsyncTask->StartSynchronousTask(EQueuedWorkPriority::Blocking);
}

void TextureSetCompilerTask::StartAsync(FQueuedThreadPool* InQueuedPool, EQueuedWorkPriority InQueuedWorkPriority)
{
	CreateDerivedData();
	AsyncTask = MakeUnique<FAsyncTask<FTextureSetCompilerTaskWorker>>(Compiler, DerivedData.Get(), bIsDefaultTextureSet);

	AsyncTask->StartBackgroundTask(InQueuedPool, InQueuedWorkPriority);
}

bool TextureSetCompilerTask::TryFinalize()
{
	// Kick off texture compilation since we should now have valid source data in the derived textures
	// (This needs to happen on the main thread, hence why it's done in TryFinalize)

	if (bHasFinalized)
		return true;

	if (!AsyncTask || !AsyncTask->IsDone())
		return false;

	// Default texture sets don't use transient source data as they're generally 4x4 so can store their source data more easily.
	if (!bIsDefaultTextureSet && !bHasAddedSourceProviders)
	{
		for (int t = 0; t < DerivedData->Textures.Num(); t++)
		{
			// Transient source data since it's faster to recompute it than caching uncompressed floating point data.
			DerivedData->Textures[t].Texture->bSourceBulkDataTransient = true;

			// Create source provider, which will fill in the source data on demand prior to a texture build 
			UTextureSetTextureSourceProvider* SourceProvider = NewObject<UTextureSetTextureSourceProvider>(DerivedData->Textures[t].Texture);
			SourceProvider->CompilerArgs = Compiler->Args;
			SourceProvider->Index = t;
			DerivedData->Textures[t].Texture->ProceduralTextureProvider = SourceProvider;
		}
		bHasAddedSourceProviders = true;
	}

	if (!bHasBeganTextureCache && FApp::CanEverRender())
	{
		for (const FDerivedTexture& DerivedTexture : DerivedData->Textures)
		{
			DerivedTexture.Texture->BeginCachePlatformData();
		}
		bHasBeganTextureCache = true;
	}

	if (FApp::CanEverRender())
	{
		for (const FDerivedTexture& DerivedTexture : DerivedData->Textures)
		{
			// It's finicky to get the texture to finish with a valid, non-default resource;
			// Wait until we're not doing any async work
			if (DerivedTexture.Texture->IsAsyncCacheComplete()) 
			{
				// Need to explicitly call FinishCachePlatformData otherwise we may have a texture stuck in limbo
				DerivedTexture.Texture->FinishCachePlatformData();
				// UpdateResource needs to be called AFTER FinishCachePlatformData
				// Otherwise it just kicks off a new build and sets the resource to a default texture
				DerivedTexture.Texture->UpdateResource();
			}
					
			if (DerivedTexture.Texture->IsDefaultTexture())
			{
				// We will continue to return false here until we have a valid texture
				return false;
			}
		}
	}

	// Ensure all source textures are freed
	if (bHasAddedSourceProviders)
	{
		for (int t = 0; t < DerivedData->Textures.Num(); t++)
		{
			Compiler->FreeTextureSource(DerivedData->Textures[t], t);
		}
	}

	bHasFinalized = true;
	return true;
}

void TextureSetCompilerTask::Finalize()
{
	check(AsyncTask);

	if (!AsyncTask->IsDone())
		AsyncTask->EnsureCompletion(true, true);

	if (!TryFinalize())
	{
		if (FApp::CanEverRender())
		{
			for (const FDerivedTexture& DerivedTexture : DerivedData->Textures)
			{
				DerivedTexture.Texture->FinishCachePlatformData();
			}
		}

		verify(TryFinalize());
	}
}

void TextureSetCompilerTask::CreateDerivedData()
{
	// May need to create UObjects, so has to execute in game thread
	check(IsInGameThread());
	check(!DerivedData.IsValid()); // Data should not have been created yet

	Compiler->LoadResources();

	const int NumDerivedTextures = Compiler->Args->PackingInfo.NumPackedTextures();

	DerivedData.Reset(NewObject<UTextureSetDerivedData>());
	DerivedData->Textures.SetNum(NumDerivedTextures);

	// Create the UTextures
	for (int t = 0; t < NumDerivedTextures; t++)
	{
		uint8 Flags = Compiler->Args->PackingInfo.GetPackedTextureInfo(t).Flags;

		TSubclassOf<UTexture> DerivedTextureClass;

		if (Flags & (uint8)ETextureSetTextureFlags::Array)
			DerivedTextureClass = UTexture2DArray::StaticClass();
		else
			DerivedTextureClass = UTexture2D::StaticClass();

		FDerivedTexture& DerivedTexture = DerivedData->Textures[t];

		FName TextureName = FName(FString::Format(TEXT("{0}_Texture_{1}"), {Compiler->Args->NamePrefix, t}));

		DerivedTexture.Texture = NewObject<UTexture>(DerivedData.Get(), DerivedTextureClass, TextureName);

		Compiler->ConfigureTexture(DerivedData->Textures[t], t);
		Compiler->InitializeTextureSource(DerivedData->Textures[t], t);
	}
}
