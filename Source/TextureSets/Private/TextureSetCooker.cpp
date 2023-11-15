// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetCooker.h"

#if WITH_EDITOR

#include "Async/Async.h"
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
		FDerivedTextureData TextureData = Cooker.BuildTextureData(CookedTextureIndex);

		OutData.Empty(2048);
		FMemoryWriter DataWriter(OutData);
		DataWriter << TextureData;
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

FTextureSetCooker::FTextureSetCooker(UTextureSet* TextureSet, FTextureSetDerivedData& DerivedData, TArray<const ITargetPlatform*> TargetPlatforms)
	: DerivedData(DerivedData)
	, TargetPlatforms(TargetPlatforms)
	, Context(TextureSet)
	, GraphInstance(nullptr)
	, PackingInfo(TextureSet->Definition->GetPackingInfo())
	, ModuleInfo(TextureSet->Definition->GetModuleInfo())
	, AsyncTask(nullptr)
{
	check(IsInGameThread());
	check(IsValid(TextureSet->Definition));
	
	OuterObject = TextureSet;
	TextureSetName = TextureSet->GetName();
	TextureSetFullName = TextureSet->GetFullName();
	UserKey = TextureSet->GetUserKey();

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
		return true;

	for (const auto& [Name, Id] : ParameterIds)
	{
		FDerivedParameterData* ParameterData = DerivedData.MaterialParameters.Find(Name);

		if (!ParameterData || (Id != ParameterData->Id))
			return true;
	}

	for (int t = 0; t < DerivedData.Textures.Num(); t++)
	{
		if (DerivedTextureIds[t] != DerivedData.TextureData[t].Id)
			return true;

		// Make sure the texture is using the right key before checking if it's cached
		ConfigureTexture(t);

		for (const ITargetPlatform* Platform : TargetPlatforms)
		{
			if (!DerivedData.Textures[t]->PlatformDataExistsInCache(Platform))
				return true;
		}
	}

	return false;
}

void FTextureSetCooker::Execute()
{
	check(!IsAsyncJobInProgress());
	check(IsInGameThread());

	Prepare();
	ExecuteInternal();
}

void FTextureSetCooker::ExecuteAsync(FQueuedThreadPool* InQueuedPool, EQueuedWorkPriority InQueuedWorkPriority)
{
	check(!IsAsyncJobInProgress());
	check(IsInGameThread());

	Prepare();
	AsyncTask = MakeUnique<FAsyncTask<FTextureSetCookerTaskWorker>>(this);
	AsyncTask->StartBackgroundTask(InQueuedPool, InQueuedWorkPriority);
}

void FTextureSetCooker::Finalize()
{
	check(IsInGameThread());
	check(!IsAsyncJobInProgress())

	for (int t = 0; t < DerivedData.Textures.Num(); t++)
	{
		// Configure textures is called again after cooking, to make sure the correct keys are used.
		// If Mip data was updated in BuildTextureData, it would reset the key incorrectly.
		ConfigureTexture(t);

		UTexture* Texture = DerivedData.Textures[t];

		for (const ITargetPlatform* Platform : TargetPlatforms)
		{
			if (Platform != nullptr)
			{
				Texture->BeginCacheForCookedPlatformData(Platform);
			}
			else
			{
				DerivedData.Textures[t]->UpdateResource();
				DerivedData.Textures[t]->UpdateCachedLODBias();
			}
		}

		// ResumeCaching() is called after we call BeginCacheForCookedPlatformData, to 
		// combine with any other possible calls that occured when caching was suspended.
		Texture->ResumeCaching();
	}

	DerivedData.bIsCooking = false;
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

	IdBuilder << FString("TextureSetDerivedTexture_V0.0"); // Version string, bump this to invalidate everything
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
	IdBuilder << FString("TextureSetParameter_V0.1"); // Version string, bump this to invalidate everything
	IdBuilder << UserKey; // Key for debugging, easily force rebuild
	IdBuilder << Parameter->ComputeGraphHash();
	IdBuilder << Parameter->ComputeDataHash(Context);
	return IdBuilder.Build();
}

void FTextureSetCooker::Prepare()
{
	// May need to create UObjects, so has to execute in game thread
	check(IsInGameThread());

	DerivedData.bIsCooking = true;

	// Garbage collection will destroy the unused cooked textures when all references from material instance are removed
	DerivedData.Textures.SetNum(PackingInfo.NumPackedTextures());
	DerivedData.TextureData.SetNum(PackingInfo.NumPackedTextures());

	for (int t = 0; t < DerivedData.Textures.Num(); t++)
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

		if (!IsValid(DerivedData.Textures[t]) || DerivedData.Textures[t].GetClass() != DerivedTextureClass)
		{
			// Try to find an existing texture stored in our package that may have become unreferenced, but still exists.
			// This could happen if the number of derived textures was higher, became lower, and then was set higher again,
			// before the previous texture was garbage-collected.
			DerivedData.Textures[t] = static_cast<UTexture*>(FindObjectWithOuter(OuterObject, nullptr, TextureName));

			// No existing texture or texture was of the wrong type, create a new one
			if (!IsValid(DerivedData.Textures[t]) || DerivedData.Textures[t].GetClass() != DerivedTextureClass)
			{
				DerivedData.Textures[t] = NewObject<UTexture>(OuterObject, DerivedTextureClass,TextureName, RF_Public);
			}
		}

		// Usually happens if the outer texture set is renamed
		if (DerivedData.Textures[t]->GetFName() != TextureName)
		{
			DerivedData.Textures[t]->Rename(*TextureName.ToString());
		}

		check(DerivedData.Textures[t]->IsInOuter(OuterObject));

		// Suspend caching to make sure the source data won't be contended.
		DerivedData.Textures[t]->SuspendCaching();

		// Derived textures must be configured before cooking, so PlatformDataExistsInCache() uses the correct keys
		ConfigureTexture(t);
	}

	// Create a new instance of the processing graph that we'll then initialize on the most recent data.
	GraphInstance = MakeShared<FTextureSetProcessingGraph>(ModuleInfo.GetModules());

	// Load all resources required by the graph
	for (const auto& [Name, TextureNode] : GraphInstance->GetOutputTextures())
		TextureNode->LoadResources(Context);

	for (const auto& [Name, ParameterNode] : GraphInstance->GetOutputParameters())
		ParameterNode->LoadResources(Context);
}

void FTextureSetCooker::ConfigureTexture(int Index) const
{
	// Configure texture MUST be called only in the game thread.
	// Changing texture properties in a thread can cause a crash in the texture compiler.
	check(IsInGameThread());
	check(!IsAsyncJobInProgress());

	const FTextureSetPackedTextureDef TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = PackingInfo.GetPackedTextureInfo(Index);

	UTexture* Texture = DerivedData.Textures[Index];

	// sRGB if possible
	Texture->SRGB = TextureInfo.HardwareSRGB;

	// Make sure the texture's compression settings are correct
	Texture->CompressionSettings = TextureDef.CompressionSettings;

	// Let the texture compression know if we don't need the alpha channel
	Texture->CompressionNoAlpha = TextureInfo.ChannelCount <= 3;

	// Set the ID of the generated source to match the hash ID of this texture.
	// This will be used to recover the cooked texture data from the DDC if possible.
	Texture->Source.SetId(DerivedTextureIds[Index], true);
	Texture->bSourceBulkDataTransient = true;
}

void FTextureSetCooker::ExecuteInternal()
{
	#ifdef BENCHMARK_TEXTURESET_COOK
		UE_LOG(LogTextureSetCook, Log, TEXT("%s: Beginning texture set cook"), *OuterObject->GetName());
		const double BuildStartTime = FPlatformTime::Seconds();
		double SectionStartTime = BuildStartTime;
	#endif

	// TODO: Log stats with "COOK_STAT" macro
	// TODO: Run DDC async
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();

	ParallelFor(DerivedTextureIds.Num(), [&](int32 t)
	{
		bool bDataWasBuilt = false;

		if (DerivedTextureIds[t] != DerivedData.TextureData[t].Id) // Only invoke DDC if keys don't match
		{
			// Retreive derived data from the DDC, or cook new data
			TArray<uint8> Data;
			if (DDC.GetSynchronous(new TextureSetDerivedTextureDataPlugin(*this, t), Data, &bDataWasBuilt))
			{
				// De-serialized the data from the cache into the derived data
				FMemoryReader DataReader(Data);
				DataReader << DerivedData.TextureData[t];
			}
		}

		for (const ITargetPlatform* Platform : TargetPlatforms)
		{
			if (!bDataWasBuilt && !DerivedData.Textures[t]->PlatformDataExistsInCache(Platform))
			{
				// This can happen if a texture build gets interrupted after a texture-set cook, or if a different platform is required.
				// Build this texture to regenerate the source data, so it can be cached.
				BuildTextureData(t);
				break;
			}
		}
	});

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

#ifdef BENCHMARK_TEXTURESET_COOK
	UE_LOG(LogTextureSetCook, Log, TEXT("%s: Cook finished in %fs"), *OuterObject->GetName(), FPlatformTime::Seconds() - SectionStartTime);
	SectionStartTime = FPlatformTime::Seconds();
#endif
}

FDerivedTextureData FTextureSetCooker::BuildTextureData(int Index) const
{
#ifdef BENCHMARK_TEXTURESET_COOK
	UE_LOG(LogTextureSetCook, Log, TEXT("%s: Beginning texture data build"), *DerivedData.Textures[Index].GetName());
	const double BuildStartTime = FPlatformTime::Seconds();
	double SectionStartTime = BuildStartTime;
#endif

	FDerivedTextureData Data;
	Data.Id = DerivedTextureIds[Index];

	const FTextureSetPackedTextureDef TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = PackingInfo.GetPackedTextureInfo(Index);
	int Width = 4;
	int Height = 4;
	int Slices = 1;
	float Ratio = 0;

	const TMap<FName, TSharedRef<ITextureProcessingNode>>& OutputTextures = GraphInstance->GetOutputTextures();

	for (int c = 0; c < TextureInfo.ChannelCount; c++)
	{
		const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

		if (!OutputTextures.Contains(ChanelInfo.ProcessedTexture))
			continue;

		const TSharedRef<ITextureProcessingNode> OutputTexture = OutputTextures.FindChecked(ChanelInfo.ProcessedTexture);

		OutputTexture->Initialize(*GraphInstance);

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

#ifdef BENCHMARK_TEXTURESET_COOK
	UE_LOG(LogTextureSetCook, Log, TEXT("%s Build: Initializing processing nodes took %fs"), *DerivedData.Textures[Index].GetName(), FPlatformTime::Seconds() - SectionStartTime);
	SectionStartTime = FPlatformTime::Seconds();
#endif

	// Copy pixel data into the texture's source
	UTexture* Texture = DerivedData.Textures[Index];
	Texture->Source.Init(Width, Height, Slices, 1, TSF_RGBA32F);

	float* PixelValues = (float*)Texture->Source.LockMip(0);
	const int PixelValueStride = 4;
	const int NumPixelValues = Width * Height * Slices * PixelValueStride;

	float MaxPixelValues[4] {};
	float MinPixelValues[4] {};

#ifdef BENCHMARK_TEXTURESET_COOK
	UE_LOG(LogTextureSetCook, Log, TEXT("%s Build: Initializing data took %fs"), *DerivedData.Textures[Index].GetName(), FPlatformTime::Seconds() - SectionStartTime);
	SectionStartTime = FPlatformTime::Seconds();
#endif

	FVector4f RestoreMul = FVector4f::One();
	FVector4f RestoreAdd = FVector4f::Zero();

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
			MaxPixelValues[c] = TNumericLimits<float>::Lowest();
			MinPixelValues[c] = TNumericLimits<float>::Max();

#if PROCESSING_METHOD == PROCESSING_METHOD_CHUNK
			// Do the whole channel as a single chunk
			FTextureProcessingChunk Chunk(
				FIntVector(0,0,0),
				FIntVector(Width - 1, Height - 1, Slices - 1),
				ChanelInfo.ProessedTextureChannel,
				c,
				PixelValueStride,
				Width,
				Height);

			ProcessedTexture->ComputeChunk(Chunk, PixelValues);

			// Pixel data is already filled, we just need to calculate the min and max values
			for (int DataIndex = Chunk.DataStart; DataIndex <= Chunk.DataEnd; DataIndex += Chunk.DataPixelStride)
			{
				const float PixelValue = PixelValues[DataIndex];
				MaxPixelValues[c] = FMath::Max(MaxPixelValues[c], PixelValue);
				MinPixelValues[c] = FMath::Min(MinPixelValues[c], PixelValue);
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
				const float Min = MinPixelValues[c];
				const float Max = MaxPixelValues[c];

				if (Min < Max)
				{
					const float CompressMul = Max - Min;
					const float CompressAdd = -Min * CompressMul;

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

#ifdef BENCHMARK_TEXTURESET_COOK
	UE_LOG(LogTextureSetCook, Log, TEXT("%s Build: Filling channels data took %fs"), *DerivedData.Textures[Index].GetName(), FPlatformTime::Seconds() - SectionStartTime);
	SectionStartTime = FPlatformTime::Seconds();
#endif

	if (RestoreMul != FVector4f::One() || RestoreAdd != FVector4f::Zero())
	{
		Data.TextureParameters.Add(TextureInfo.RangeCompressMulName, RestoreMul);
		Data.TextureParameters.Add(TextureInfo.RangeCompressAddName, RestoreAdd);
	}

	Texture->Source.UnlockMip(0);

#ifdef BENCHMARK_TEXTURESET_COOK
	const double BuildEndTime = FPlatformTime::Seconds();
	UE_LOG(LogTextureSetCook, Log, TEXT("%s: Texture data build took %fs"), *DerivedData.Textures[Index].GetName(), BuildEndTime - BuildStartTime);
#endif
	return Data;
}
#endif