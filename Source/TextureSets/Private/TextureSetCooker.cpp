// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetCooker.h"

#if WITH_EDITOR

#include "Async/Async.h"
#include "DerivedDataBuildVersion.h"
#include "DerivedDataCacheInterface.h"
#include "ProcessingNodes/TextureInput.h"
#include "ProcessingNodes/TextureOperatorEnlarge.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetDerivedData.h"

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
	virtual bool IsBuildThreadsafe() const override { return false; } // TODO: Enable this if possible
	virtual bool IsDeterministic() const override { return false; } // TODO: Enable this if possible
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
	const FTextureSetCooker& Cooker;
	const int32 CookedTextureIndex;
};

class TextureSetDerivedParameterDataPlugin : public FDerivedDataPluginInterface
{
public:
	TextureSetDerivedParameterDataPlugin(const FTextureSetCooker& Cooker, FName ParameterName)
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
		TSharedRef<IParameterProcessingNode> Parameter = Cooker.Graph.GetOutputParameters().FindChecked(ParameterName);

		Parameter->Initialize(Cooker.Context);

		FDerivedParameterData ParameterData;
		ParameterData.Value = Parameter->GetValue();
		ParameterData.Id = Cooker.ParameterIds.FindChecked(ParameterName);

		OutData.Empty(sizeof(FDerivedParameterData));
		FMemoryWriter DataReader(OutData);
		DataReader << ParameterData;
		return true;
	}

private:
	const FTextureSetCooker& Cooker;
	const FName ParameterName;
};

FTextureSetCooker::FTextureSetCooker(UTextureSet* TextureSet, FTextureSetDerivedData& DerivedData)
	: DerivedData(DerivedData)
	, Context(TextureSet)
	, Graph(FTextureSetProcessingGraph(TextureSet->Definition->GetModules()))
	, ModuleInfo(TextureSet->Definition->GetModuleInfo())
	, PackingInfo(TextureSet->Definition->GetPackingInfo())
	, AsyncTask(nullptr)
{
	check(IsInGameThread());
	check(IsValid(TextureSet->Definition));
	
	OuterObject = TextureSet;
	TextureSetName = TextureSet->GetName();
	TextureSetFullName = TextureSet->GetFullName();
	UserKey = TextureSet->GetUserKey();

	for (int i = 0; i < PackingInfo.NumPackedTextures(); i++)
		DerivedTextureIds.Add(ComputeTextureDataId(i));

	for (const auto& [Name, Parameter] : Graph.GetOutputParameters())
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

		if (!DerivedData.Textures[t]->PlatformDataExistsInCache())
			return true;
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

		// Update resources re-caches the derived texture data
		DerivedData.Textures[t]->UpdateResource();
		DerivedData.Textures[t]->UpdateCachedLODBias();
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

FGuid FTextureSetCooker::ComputeTextureDataId(int PackedTextureIndex) const
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
		TSharedRef<ITextureProcessingNode> TextureNode = Graph.GetOutputTextures().FindChecked(TextureName);
		IdBuilder << TextureNode->ComputeGraphHash();
		IdBuilder << TextureNode->ComputeDataHash(Context);
	}

	return IdBuilder.Build();
}

FGuid FTextureSetCooker::ComputeParameterDataId(const TSharedRef<IParameterProcessingNode> Parameter) const
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
		FName TextureName = FName(TextureSetName + TEXT("_DerivedTexture_") + FString::FromInt(t));

		if (!IsValid(DerivedData.Textures[t]))
		{
			// Try to find an existing texture stored in our package that may have become unreferenced, but still exists.
			// This could happen if the number of derived textures was higher, became lower, and then was set higher again,
			// before the previous texture was garbage-collected.
			DerivedData.Textures[t] = static_cast<UTexture*>(FindObjectWithOuter(OuterObject, nullptr, TextureName));

			// No existing texture, create a new one
			if (!IsValid(DerivedData.Textures[t]))
			{
				DerivedData.Textures[t] = NewObject<UTexture2D>(OuterObject, TextureName, RF_Public);
			}
		}

		// Usually happens if the outer texture set is renamed
		if (DerivedData.Textures[t]->GetFName() != TextureName)
		{
			DerivedData.Textures[t]->Rename(*TextureName.ToString());
		}

		check(DerivedData.Textures[t]->IsInOuter(OuterObject));

		// Derived textures must be configured before cooking, so PlatformDataExistsInCache() uses the correct keys
		ConfigureTexture(t);
	}

	// Load all resources required by the graph
	for (const auto& [Name, TextureNode] : Graph.GetOutputTextures())
		TextureNode->LoadResources(Context);

	for (const auto& [Name, ParameterNode] : Graph.GetOutputParameters())
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
	// TODO: Log stats with "COOK_STAT" macro
	// TODO: Run DDC async
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();

	for (int t = 0; t < DerivedTextureIds.Num(); t++)
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

		if (!bDataWasBuilt && !DerivedData.Textures[t]->PlatformDataExistsInCache())
		{
			// This can happen if a texture build gets interrupted after a texture-set cook, or if a different platform is required.
			// Build this texture to regenerate the source data, so it can be cached.
			BuildTextureData(t);
		}
	}

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
}

FDerivedTextureData FTextureSetCooker::BuildTextureData(int Index) const
{
	FDerivedTextureData Data;
	Data.Id = DerivedTextureIds[Index];

	const FTextureSetPackedTextureDef TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = PackingInfo.GetPackedTextureInfo(Index);
	int Width = 4;
	int Height = 4;
	float Ratio = 0;

	const TMap<FName, TSharedRef<ITextureProcessingNode>>& OutputTextures = Graph.GetOutputTextures();

	for (int c = 0; c < TextureInfo.ChannelCount; c++)
	{
		const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

		if (!OutputTextures.Contains(ChanelInfo.ProcessedTexture))
			continue;

		const TSharedRef<ITextureProcessingNode> OutputTexture = OutputTextures.FindChecked(ChanelInfo.ProcessedTexture);

		OutputTexture->Initialize(Context);

		const int ChannelWidth = OutputTexture->GetWidth();
		const int ChannelHeight = OutputTexture->GetHeight();
		const float NewRatio = (float)Width / (float)Height;

		// Calculate the maximum size of all of our processed textures. We'll use this as our packed texture size.
		Width = FMath::Max(Width, ChannelWidth);
		Height = FMath::Max(Height, ChannelHeight);
		// Verify that all processed textures have the same aspect ratio
		check(NewRatio == Ratio || Ratio == 0.0f);
		Ratio = NewRatio;
	}

	// Copy pixel data into the texture's source
	UTexture* Texture = DerivedData.Textures[Index];
	Texture->Source.Init(Width, Height, 1, 1, TSF_RGBA16F);
	FFloat16* PixelValues = (FFloat16*)Texture->Source.LockMip(0);

	float MaxPixelValues[4] {};
	float MinPixelValues[4] {};

	// TODO: Execute each channel in ParallelFor?
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
				ProcessedTexture = MakeShared<FTextureOperatorEnlarge>(ProcessedTexture, Width, Height);
			}

			// Initialize the max and min pixel values so they will be overridden by the first pixel
			MaxPixelValues[c] = TNumericLimits<float>::Lowest();
			MinPixelValues[c] = TNumericLimits<float>::Max();

			for (int x = 0; x < Width; x++)
			{
				for (int y = 0; y < Height; y++)
				{
					int PixelIndex = GetPixelIndex(x, y, c, Width, Height);

					float PixelValue = ProcessedTexture->GetPixel(x, y, ChanelInfo.ProessedTextureChannel);
					MaxPixelValues[c] = FMath::Max(MaxPixelValues[c], PixelValue);
					MinPixelValues[c] = FMath::Min(MinPixelValues[c], PixelValue);
					PixelValues[PixelIndex] = PixelValue;
				}
			}
		}
		else
		{
			// Fill unused channels with black
			for (int x = 0; x < Width; x++)
			{
				for (int y = 0; y < Height; y++)
				{
					int PixelIndex = GetPixelIndex(x, y, c, Width, Height);

					PixelValues[PixelIndex] = 0.0f;
				}
			}
		}
	}

	// Channel encoding (decoding happens in FTextureSetMaterialGraphBuilder::MakeTextureSamplerCustomNode)
	{
		FVector4f RestoreMul = FVector4f::One();
		FVector4f RestoreAdd = FVector4f::Zero();

		for (int c = 0; c < TextureInfo.ChannelCount; c++)
		{
			if (TextureInfo.ChannelInfo[c].ChannelEncoding == ETextureSetTextureChannelEncoding::Linear_RangeCompressed)
			{
				const float Min = MinPixelValues[c];
				const float Max = MaxPixelValues[c];

				if (Min >= Max) // Can happen if the texture is a solid fill
					continue;

				for (int x = 0; x < Width; x++)
				{
					for (int y = 0; y < Height; y++)
					{
						const int PixelIndex = GetPixelIndex(x, y, c, Width, Height);

						PixelValues[PixelIndex] = (PixelValues[PixelIndex] - Min) * Max - Min;
					}
				}

				RestoreMul[c] = 1.0f / (Max - Min);
				RestoreAdd[c] = Min;
			}
			else if (TextureInfo.ChannelInfo[c].ChannelEncoding == ETextureSetTextureChannelEncoding::SRGB && (!TextureInfo.HardwareSRGB || c >= 3))
			{
				for (int x = 0; x < Width; x++)
				{
					for (int y = 0; y < Height; y++)
					{
						const int PixelIndex = GetPixelIndex(x, y, c, Width, Height);

						PixelValues[PixelIndex] = FMath::Pow(PixelValues[PixelIndex], 1.0f / 2.2f);
					}
				}
			}
		}

		if (RestoreMul != FVector4f::One() || RestoreAdd != FVector4f::Zero())
		{
			Data.TextureParameters.Add(TextureInfo.RangeCompressMulName, RestoreMul);
			Data.TextureParameters.Add(TextureInfo.RangeCompressAddName, RestoreAdd);
		}
	}

	Texture->Source.UnlockMip(0);
	return Data;
}
#endif