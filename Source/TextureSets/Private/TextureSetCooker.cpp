// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetCooker.h"

#if WITH_EDITOR

#include "Async/ParallelFor.h"
#include "DerivedDataBuildVersion.h"
#include "DerivedDataCacheInterface.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "ProcessingNodes/TextureInput.h"
#include "ProcessingNodes/TextureOperatorEnlarge.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetDerivedData.h"
#include "TextureSetProcessingContext.h"
#include "TextureSetTextureSourceProvider.h"
#include "TextureCompiler.h"

#define BENCHMARK_TEXTURESET_COOK

DEFINE_LOG_CATEGORY(LogTextureSetCook);

static TAutoConsoleVariable<int32> CVarTextureSetParallelCook(
	TEXT("r.TextureSet.ParallelCook"),
	1,
	TEXT("Execute the texture cooking across multiple threads in parallel when possible"),
	ECVF_Default);

void FTextureSetCookerTaskWorker::DoWork()
{
	Cooker->ExecuteInternal();
}

class TextureSetDerivedTextureDataPlugin : public FDerivedDataPluginInterface
{
public:
	TextureSetDerivedTextureDataPlugin(FTextureSetCooker& Cooker, int32 CookedTextureIndex)
		: Cooker(Cooker)
		, CookedTextureIndex(CookedTextureIndex)
	{}

	// FDerivedDataPluginInterface
	virtual const TCHAR* GetPluginName() const override { return TEXT("TextureSet_FDerivedTextureData"); }
	virtual const TCHAR* GetVersionString() const override { return TEXT("F956B1B9-3AD3-47B3-BD82-8826C71A3DCB"); }
	virtual FString GetPluginSpecificCacheKeySuffix() const override { return Cooker.DerivedTextureIds[CookedTextureIndex].ToString(); }
	virtual bool IsBuildThreadsafe() const override { return true; }
	virtual bool IsDeterministic() const override { return true; }
	virtual FString GetDebugContextString() const override { return Cooker.TextureSetFullName; }
	
	virtual bool Build(TArray<uint8>& OutData) override
	{
		Cooker.InitializeTextureData(CookedTextureIndex);
		Cooker.BuildTextureData(CookedTextureIndex);

		OutData.Empty(2048);
		FMemoryWriter DataWriter(OutData);
		DataWriter << Cooker.DerivedData.TextureData[CookedTextureIndex];
		return true;
	}

private:
	FTextureSetCooker& Cooker;
	const int32 CookedTextureIndex;
};

class TextureSetDerivedParameterDataPlugin : public FDerivedDataPluginInterface
{
public:
	TextureSetDerivedParameterDataPlugin(FTextureSetCooker& Cooker, FName ParameterName)
		: Cooker(Cooker)
		, ParameterName(ParameterName)
	{}

	// FDerivedDataPluginInterface
	virtual const TCHAR* GetPluginName() const override { return TEXT("TextureSet_FDerivedTextureData"); }
	virtual const TCHAR* GetVersionString() const override { return TEXT("8D5EAAD9-8514-4957-B931-C7AF2698794F"); }
	virtual FString GetPluginSpecificCacheKeySuffix() const override { return Cooker.ParameterIds.FindChecked(ParameterName).ToString(); }
	virtual bool IsBuildThreadsafe() const override { return false; } // TODO: Enable this if possible
	virtual bool IsDeterministic() const override { return false; } // TODO: Enable this if possible
	virtual FString GetDebugContextString() const override { return Cooker.TextureSetFullName; }
	virtual bool Build(TArray<uint8>& OutData) override
	{
		TSharedRef<IParameterProcessingNode> Parameter = Cooker.GraphInstance->GetOutputParameters().FindChecked(ParameterName);

		Parameter->Initialize(*Cooker.GraphInstance);

		FDerivedParameterData ParameterData;
		ParameterData.Value = Parameter->GetValue();
		ParameterData.Id = Cooker.ParameterIds.FindChecked(ParameterName);

		OutData.Empty(sizeof(FDerivedParameterData));
		FMemoryWriter DataReader(OutData);
		DataReader << ParameterData;
		return true;
	}

private:
	FTextureSetCooker& Cooker;
	const FName ParameterName;
};

FTextureSetCooker::FTextureSetCooker(UTextureSet* TextureSet, FTextureSetDerivedData& DerivedData)
	: DerivedData(DerivedData)
	, Context(TextureSet)
	, GraphInstance(nullptr)
	, PackingInfo(TextureSet->Definition->GetPackingInfo())
	, ModuleInfo(TextureSet->Definition->GetModuleInfo())
	, bIsDefaultTextureSet(TextureSet->Definition->GetDefaultTextureSet() == TextureSet)
	, AsyncTask(nullptr)
{
	check(IsInGameThread());
	check(IsValid(TextureSet->Definition));
	check(DerivedData.bIsCompiling == false);
	
	OuterObject = TextureSet;
	TextureSetName = TextureSet->GetName();
	TextureSetFullName = TextureSet->GetFullName();
	UserKey = TextureSet->GetUserKey() + TextureSet->Definition->GetUserKey();

	// Compute all IDs using the instance of the processing graph already created in the module info.
	// This saves us having to create a new processing graph just to do our hashing.
	const FTextureSetProcessingGraph* Graph = ModuleInfo.GetProcessingGraph();

	for (int i = 0; i < PackingInfo.NumPackedTextures(); i++)
		DerivedTextureIds.Add(ComputeTextureDataId(i, Graph->GetOutputTextures()));

	for (const auto& [Name, Parameter] : Graph->GetOutputParameters())
		ParameterIds.Add(Name, ComputeParameterDataId(Parameter));
}

bool FTextureSetCooker::CookRequired() const
{
	check(IsInGameThread());

	if (DerivedData.Textures.Num() != PackingInfo.NumPackedTextures())
		return true; // Needs to add or remove a derived texture

	if (DerivedData.TextureData.Num() != PackingInfo.NumPackedTextures())
		return true; // Needs to add or remove derived texture data

	for (const auto& [Name, Id] : ParameterIds)
	{
		FDerivedParameterData* ParameterData = DerivedData.MaterialParameters.Find(Name);

		if (!ParameterData || (Id != ParameterData->Id))
			return true; // Some parameter data needs to be updated
	}

	for (int t = 0; t < DerivedData.Textures.Num(); t++)
	{
		FDerivedTextureData& TextureData = DerivedData.TextureData[t];

		if (DerivedTextureIds[t] != TextureData.Id)
			return true; // Some texture data needs to be updated
	}

	return false;
}

void FTextureSetCooker::Execute()
{
	check(!IsAsyncJobInProgress());
	check(IsInGameThread());
	DerivedData.bIsCompiling = true;
	ExecuteInternal();
}

void FTextureSetCooker::ExecuteAsync(FQueuedThreadPool* InQueuedPool, EQueuedWorkPriority InQueuedWorkPriority)
{
	check(!IsAsyncJobInProgress());
	check(IsInGameThread());
	DerivedData.bIsCompiling = true;
	AsyncTask = MakeUnique<FAsyncTask<FTextureSetCookerTaskWorker>>(this);
	AsyncTask->StartBackgroundTask(InQueuedPool, InQueuedWorkPriority);
}

bool FTextureSetCooker::IsAsyncJobInProgress() const
{
	check(IsInGameThread());
	return AsyncTask.IsValid() && !AsyncTask->IsDone();
}

bool FTextureSetCooker::TryCancel()
{
	check(IsInGameThread());
	if (IsAsyncJobInProgress())
		return AsyncTask->Cancel();
	else
		return true;
}

FGuid FTextureSetCooker::ComputeTextureDataId(int PackedTextureIndex, const TMap<FName, TSharedRef<ITextureProcessingNode>>& ProcessedTextures) const
{
	check(PackedTextureIndex < PackingInfo.NumPackedTextures());

	UE::DerivedData::FBuildVersionBuilder IdBuilder;

	IdBuilder << FString("TextureSetDerivedTexture_V0.3"); // Version string, bump this to invalidate everything
	IdBuilder << UserKey; // Key for debugging, easily force rebuild
	IdBuilder << GetTypeHash(PackingInfo.GetPackedTextureDef(PackedTextureIndex));

	TSet<FName> TextureDependencies;
	for (const FTextureSetPackedChannelInfo& ChannelInfo : PackingInfo.GetPackedTextureInfo(PackedTextureIndex).ChannelInfo)
	{
		if (!ChannelInfo.ProcessedTexture.IsNone())
			TextureDependencies.Add(ChannelInfo.ProcessedTexture);
	}

	// Only hash on source textures that contribute to this packed texture
	for (const FName& TextureName : TextureDependencies)
	{
		const TSharedRef<ITextureProcessingNode>& TextureNode = ProcessedTextures.FindChecked(TextureName);
		IdBuilder << TextureNode->ComputeGraphHash();
		IdBuilder << TextureNode->ComputeDataHash(Context);
	}

	return IdBuilder.Build();
}

FGuid FTextureSetCooker::ComputeParameterDataId(const IParameterProcessingNode* Parameter) const
{
	UE::DerivedData::FBuildVersionBuilder IdBuilder;
	IdBuilder << FString("TextureSetParameter_V0.2"); // Version string, bump this to invalidate everything
	IdBuilder << UserKey; // Key for debugging, easily force rebuild
	IdBuilder << Parameter->ComputeGraphHash();
	IdBuilder << Parameter->ComputeDataHash(Context);
	return IdBuilder.Build();
}

void FTextureSetCooker::Prepare()
{
	check(IsInGameThread());

	// May need to create UObjects, so has to execute in game thread
	check(!IsAsyncJobInProgress());

	// Create a new instance of the processing graph that we'll then initialize on the most recent data.
	GraphInstance = MakeShared<FTextureSetProcessingGraph>(ModuleInfo.GetModules());

	// Load all resources required by the graph
	for (const auto& [Name, TextureNode] : GraphInstance->GetOutputTextures())
		TextureNode->LoadResources(Context);

	for (const auto& [Name, ParameterNode] : GraphInstance->GetOutputParameters())
		ParameterNode->LoadResources(Context);

	const int NumDerivedTextures = PackingInfo.NumPackedTextures();

	// Garbage collection will destroy the unused cooked textures when all references from material instance are removed
	DerivedData.Textures.SetNum(NumDerivedTextures);
	DerivedData.TextureData.SetNum(NumDerivedTextures);
	TextureDataCS.SetNum(NumDerivedTextures);
	TextureStates.SetNumZeroed(NumDerivedTextures);

	for (int t = 0; t < NumDerivedTextures; t++)
	{
		uint8 Flags = PackingInfo.GetPackedTextureInfo(t).Flags;

		TSubclassOf<UTexture> DerivedTextureClass;
		FString DerivedTextureSuffix;

		if (Flags & (uint8)ETextureSetTextureFlags::Array)
		{
			DerivedTextureSuffix = "2DA";
			DerivedTextureClass = UTexture2DArray::StaticClass();
		}
		else
		{
			DerivedTextureSuffix = "2D";
			DerivedTextureClass = UTexture2D::StaticClass();
		}

		FName TextureName = FName(FString::Format(TEXT("{0}_Texture{1}_{2}"), {TextureSetName, DerivedTextureSuffix, t}));

		TObjectPtr<UTexture>& Texture = DerivedData.Textures[t];

		if (!IsValid(Texture) || Texture.GetClass() != DerivedTextureClass)
		{
			// Try to find an existing texture stored in our package that may have become unreferenced, but still exists.
			// This could happen if the number of derived textures was higher, became lower, and then was set higher again,
			// before the previous texture was garbage-collected.
			Texture = static_cast<UTexture*>(FindObjectWithOuter(OuterObject, nullptr, TextureName));

			// No existing texture or texture was of the wrong type, create a new one
			if (!IsValid(Texture) || Texture.GetClass() != DerivedTextureClass)
			{
				Texture = NewObject<UTexture>(OuterObject, DerivedTextureClass, TextureName, RF_NoFlags);
			}
		}

		// Default texture set derived textures need to be public so they can be referenced as default textures
		// in the generated graphs. Otherwise, derived textures shouldn't be directly referenced
		Texture->SetFlags(bIsDefaultTextureSet ? RF_Public : RF_NoFlags);

		// Before applying any modification to the texture
		// make sure no compilation is still ongoing.
		if (!Texture->IsAsyncCacheComplete())
			Texture->FinishCachePlatformData();

		if (Texture->IsDefaultTexture())
			FTextureCompilingManager::Get().FinishCompilation({Texture});

		// Usually happens if the outer texture set is renamed
		if (Texture->GetFName() != TextureName)
			Texture->Rename(*TextureName.ToString());

		check(Texture->IsInOuter(OuterObject));

		// Create a source provider and assign it if it doesn't already exist
		if (!IsValid(Cast<UTextureSetTextureSourceProvider>(Texture->ProceduralTextureProvider)))
		{
			Texture->ProceduralTextureProvider = NewObject<UTextureSetTextureSourceProvider>(Texture);
		}

		ConfigureTexture(t);

		// In editor, kick off texture builds so we see our results right away
		if (FApp::CanEverRender())
			Texture->BeginCachePlatformData();
	}
}

void FTextureSetCooker::ConfigureTexture(int Index) const
{
	// Configure texture MUST be called only in the game thread.
	// Changing texture properties in a thread can cause a crash in the texture compiler.
	check(IsInGameThread());

	const FTextureSetPackedTextureDef TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = PackingInfo.GetPackedTextureInfo(Index);

	UTexture* Texture = DerivedData.Textures[Index];

	// sRGB if possible
	Texture->SRGB = TextureInfo.HardwareSRGB;

	// Make sure the texture's compression settings are correct
	Texture->CompressionSettings = TextureDef.CompressionSettings;

	// Let the texture compression know if we don't need the alpha channel
	Texture->CompressionNoAlpha = TextureInfo.ChannelCount <= 3;

	Texture->VirtualTextureStreaming = TextureDef.bVirtualTextureStreaming;
	// Set the ID of the generated source to match the hash ID of this texture.
	// This will be used to recover the cooked texture data from the DDC if possible.
	Texture->Source.SetId(DerivedTextureIds[Index], true);

	// Transient source data since it's faster to recompute it than caching uncompressed floating point data.
	Texture->bSourceBulkDataTransient = true;

	// Important that this is set properly for streaming
	Texture->SetDeterministicLightingGuid();

	if (TextureStates[Index] == ETextureState::Unmodified)
		TextureStates[Index] = ETextureState::Configured;
}

void FTextureSetCooker::ExecuteInternal()
{
	check(DerivedData.bIsCompiling == true);

	#ifdef BENCHMARK_TEXTURESET_COOK
		UE_LOG(LogTextureSetCook, Log, TEXT("%s: Beginning texture set cook"), *OuterObject->GetName());
		const double BuildStartTime = FPlatformTime::Seconds();
		double SectionStartTime = BuildStartTime;
	#endif

	// TODO: Log stats with "COOK_STAT" macro
	// TODO: Run DDC async
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();

	ParallelForWithPreWork(
		DerivedTextureIds.Num(),
		[&](int32 t) // Parallel For
		{	
			if (DerivedTextureIds[t] != DerivedData.TextureData[t].Id) // Only invoke DDC if keys don't match
			{
				// Retreive derived data from the DDC, or cook new data
				TArray<uint8> Data;
				bool bDataWasBuilt = false;
				if (DDC.GetSynchronous(new TextureSetDerivedTextureDataPlugin(*this, t), Data, &bDataWasBuilt))
				{
					// Don't need to deserialize if data was built, as it will have already been filled into the derived data.
					if (!bDataWasBuilt)
					{
						// De-serialized the data from the cache into the derived data
						FMemoryReader DataReader(Data);
						DataReader << DerivedData.TextureData[t];
					}
				}
			}
		},
		[&]() // Pre-Work
		{
			for (const auto& [Name, Id] : ParameterIds)
			{
				FDerivedParameterData* OldParameterData = DerivedData.MaterialParameters.Find(Name);

				if (!OldParameterData || (Id != OldParameterData->Id)) // Only invoke DDC param is missing or keys don't match
				{
					// Retreive derived data from the DDC, or cook new data
					TArray<uint8> Data;
					if (DDC.GetSynchronous(new TextureSetDerivedParameterDataPlugin(*this, Name), Data))
					{
						// De-serialized the data from the cache into the derived data
						FDerivedParameterData NewParameterData;
						FMemoryReader DataReader(Data);
						DataReader << NewParameterData;
						DerivedData.MaterialParameters.Emplace(Name, NewParameterData);
					}
				}
			}
		});

#ifdef BENCHMARK_TEXTURESET_COOK
	UE_LOG(LogTextureSetCook, Log, TEXT("%s: Cook finished in %fs"), *OuterObject->GetName(), FPlatformTime::Seconds() - SectionStartTime);
	SectionStartTime = FPlatformTime::Seconds();
#endif

	check(DerivedData.bIsCompiling == true);
	DerivedData.bIsCompiling = false;
}

void FTextureSetCooker::InitializeTextureData(int Index) const
{
	FScopeLock Lock(&TextureDataCS[Index]);

	FTextureSource& Source = DerivedData.Textures[Index]->Source;

	// Must be configured before initializing
	check(TextureStates[Index] >= ETextureState::Configured);

	if (TextureStates[Index] >= ETextureState::Initialized)
	{
		// We are already initialized, just sanity check
		// Was already built and should be up to date, just sanity check
		check(Source.IsValid());
		check(Source.GetFormat() == TSF_RGBA32F);
		return;
	}

	const FTextureSetPackedTextureDef TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = PackingInfo.GetPackedTextureInfo(Index);
	int Width = 4;
	int Height = 4;
	int Slices = 1;
	int Mips = 1;
	float Ratio = 0;

	const TMap<FName, TSharedRef<ITextureProcessingNode>>& OutputTextures = GraphInstance->GetOutputTextures();

	for (int c = 0; c < TextureInfo.ChannelCount; c++)
	{
		const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

		if (!OutputTextures.Contains(ChanelInfo.ProcessedTexture))
			continue;

		const TSharedRef<ITextureProcessingNode> OutputTexture = OutputTextures.FindChecked(ChanelInfo.ProcessedTexture);

		const int ChannelWidth = OutputTexture->GetWidth();
		const int ChannelHeight = OutputTexture->GetHeight();
		const int ChannelSlices = OutputTexture->GetSlices();
		const float NewRatio = (float)ChannelWidth / (float)ChannelHeight;

		// Calculate the maximum size of all of our processed textures. We'll use this as our packed texture size.
		Width = FMath::Max(Width, ChannelWidth);
		Height = FMath::Max(Height, ChannelHeight);
		Slices = FMath::Max(Slices, ChannelSlices);
		// Verify that all processed textures have the same aspect ratio
		check(NewRatio == Ratio || Ratio == 0.0f);
		Ratio = NewRatio;
	};

	if (!Source.IsValid() || Source.GetSizeX() != Width || Source.GetSizeY() != Height || Source.GetNumSlices() != Slices || Source.GetNumMips() != Mips || Source.GetFormat() != TSF_RGBA32F)
	{
		Source.Init(Width, Height, Slices, Mips, TSF_RGBA32F);
		
		// Initializing source resets the ID, so put it back
		Source.SetId(DerivedTextureIds[Index], true);
	}

	TextureStates[Index] = ETextureState::Initialized;
}

void FTextureSetCooker::BuildTextureData(int Index) const
{
	FScopeLock Lock(&TextureDataCS[Index]);

	// Must be initialized before building
	check(TextureStates[Index] >= ETextureState::Initialized);

	if (TextureStates[Index] >= ETextureState::Built)
	{
		// Was already built and should be up to date
		return;
	}

#ifdef BENCHMARK_TEXTURESET_COOK
	UE_LOG(LogTextureSetCook, Log, TEXT("%s: Beginning texture data build"), *DerivedData.Textures[Index].GetName());
	const double BuildStartTime = FPlatformTime::Seconds();
	double SectionStartTime = BuildStartTime;
#endif

	UTexture* Texture = DerivedData.Textures[Index];

	const FTextureSetPackedTextureDef TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = PackingInfo.GetPackedTextureInfo(Index);
	int Width = Texture->Source.GetSizeX();
	int Height = Texture->Source.GetSizeY();
	int Slices = Texture->Source.GetNumSlices();
	check(Texture->Source.GetFormat() == ETextureSourceFormat::TSF_RGBA32F);
	const int PixelValueStride = 4;
	const int NumPixelValues = Width * Height * Slices * PixelValueStride;

	const TMap<FName, TSharedRef<ITextureProcessingNode>>& OutputTextures = GraphInstance->GetOutputTextures();

	for (int c = 0; c < TextureInfo.ChannelCount; c++)
	{
		const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

		if (!OutputTextures.Contains(ChanelInfo.ProcessedTexture))
			continue;

		const TSharedRef<ITextureProcessingNode> OutputTexture = OutputTextures.FindChecked(ChanelInfo.ProcessedTexture);

		OutputTexture->Initialize(*GraphInstance);
	}

	FVector4f RestoreMul = FVector4f::One();
	FVector4f RestoreAdd = FVector4f::Zero();

#ifdef BENCHMARK_TEXTURESET_COOK
	UE_LOG(LogTextureSetCook, Log, TEXT("%s Build: Initializing data took %fs"), *DerivedData.Textures[Index].GetName(), FPlatformTime::Seconds() - SectionStartTime);
	SectionStartTime = FPlatformTime::Seconds();
#endif

	float* PixelValues = (float*)Texture->Source.LockMip(0);
	check(PixelValues != nullptr);

	for (int c = 0; c < 4; c++)
	{
		// Copy processed textures into packed textures
		const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

		if (c < TextureInfo.ChannelCount && OutputTextures.Contains(ChanelInfo.ProcessedTexture))
		{
			TSharedRef<ITextureProcessingNode> ProcessedTexture = OutputTextures.FindChecked(ChanelInfo.ProcessedTexture);

			check(ProcessedTexture->GetWidth() <= Width);
			check(ProcessedTexture->GetHeight() <= Height);

			if (ProcessedTexture->GetWidth() < Width || ProcessedTexture->GetHeight() < Height)
			{
				ProcessedTexture = MakeShared<FTextureOperatorEnlarge>(ProcessedTexture, Width, Height, Slices);
			}

			// Initialize the max and min pixel values so they will be overridden by the first pixel
			float Min = TNumericLimits<float>::Max();
			float Max = TNumericLimits<float>::Lowest();

#if PROCESSING_METHOD == PROCESSING_METHOD_CHUNK
			// Do the whole channel as a single chunk
			FTextureProcessingChunk Chunk(
				FIntVector(0,0,0),
				FIntVector(Width - 1, Height - 1, Slices - 1),
				ChanelInfo.ProessedTextureChannel,
				c,
				PixelValueStride,
				Width,
				Height,
				Slices);

			ProcessedTexture->ComputeChunk(Chunk, PixelValues);

			// Pixel data is already filled, we just need to calculate the min and max values
			for (int DataIndex = Chunk.DataStart; DataIndex <= Chunk.DataEnd; DataIndex += Chunk.DataPixelStride)
			{
				const float PixelValue = PixelValues[DataIndex];
				Min = FMath::Min(Min, PixelValue);
				Max = FMath::Max(Max, PixelValue);
			}
#else
			for (int x = 0; x < Width; x++)
			{
				for (int y = 0; y < Height; y++)
				{
					for (int z = 0; z < Slices; z++)
					{
						int PixelIndex = GetPixelIndex(x, y, z, c, Width, Height, PixelValueStride);

						float PixelValue = ProcessedTexture->GetPixel(x, y, z, ChanelInfo.ProessedTextureChannel);
						MaxPixelValues[c] = FMath::Max(MaxPixelValues[c], PixelValue);
						MinPixelValues[c] = FMath::Min(MinPixelValues[c], PixelValue);
						PixelValues[PixelIndex] = PixelValue;
					}
				}
			}
#endif

			// Channel encoding (decoding happens in FTextureSetMaterialGraphBuilder::MakeTextureSamplerCustomNode)
			if (ChanelInfo.ChannelEncoding & (uint8)ETextureSetChannelEncoding::RangeCompression)
			{
				if (Min >= Max)
				{
					// Essentially ignore the texture at runtime and use the min value
					RestoreMul[c] = 0;
					RestoreAdd[c] = Min;
				}
				else
				{
					// Adjust the texture and set the constant values for decompression
					float CompressMul = Max - Min;
					float CompressAdd = -Min * CompressMul;

					for (int i = c; i < NumPixelValues; i += PixelValueStride)
					{
						PixelValues[i] = PixelValues[i] * CompressMul + CompressAdd;
					}

					RestoreMul[c] = 1.0f / (Max - Min);
					RestoreAdd[c] = Min;
				}
			}

			if ((ChanelInfo.ChannelEncoding & (uint8)ETextureSetChannelEncoding::SRGB) && (!TextureInfo.HardwareSRGB || c >= 3))
			{
				for (int i = c; i < NumPixelValues; i += PixelValueStride)
				{
					PixelValues[i] = FMath::Pow(PixelValues[i], 1.0f / 2.2f);
				}
			}

		}
		else
		{
			if (c < 3)
			{
				for (int i = c; i < NumPixelValues; i += PixelValueStride)
				{
					PixelValues[i] = 0.0f; // Fill unused RGB with black
				}
			}
			else
			{
				for (int i = c; i < NumPixelValues; i += PixelValueStride)
				{
					PixelValues[i] = 1.0f; // Fill unused Alpha with white
				}
			}
		}
	};

	Texture->Source.UnlockMip(0);

	// UnlockMip causes GUID to be set from a hash, so force it back to the one we want to use
	Texture->Source.SetId(DerivedTextureIds[Index], true);

#ifdef BENCHMARK_TEXTURESET_COOK
	UE_LOG(LogTextureSetCook, Log, TEXT("%s Build: Filling channels data took %fs"), *DerivedData.Textures[Index].GetName(), FPlatformTime::Seconds() - SectionStartTime);
	SectionStartTime = FPlatformTime::Seconds();
#endif

	FDerivedTextureData& Data = DerivedData.TextureData[Index];
	Data.Id = DerivedTextureIds[Index];

	if (RestoreMul != FVector4f::One() || RestoreAdd != FVector4f::Zero())
	{
		Data.TextureParameters.Add(TextureInfo.RangeCompressMulName, RestoreMul);
		Data.TextureParameters.Add(TextureInfo.RangeCompressAddName, RestoreAdd);
	}
	TextureStates[Index] = ETextureState::Built;

#ifdef BENCHMARK_TEXTURESET_COOK
	const double BuildEndTime = FPlatformTime::Seconds();
	UE_LOG(LogTextureSetCook, Log, TEXT("%s: Texture data build took %fs"), *DerivedData.Textures[Index].GetName(), BuildEndTime - BuildStartTime);
#endif
}
#endif