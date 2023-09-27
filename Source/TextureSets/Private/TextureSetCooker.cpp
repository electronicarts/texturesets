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

TextureSetDerivedDataPlugin::TextureSetDerivedDataPlugin(TextureSetCooker* CookerPtr)
	: Cooker(CookerPtr)
{
}

const TCHAR* TextureSetDerivedDataPlugin::GetPluginName() const
{
	return TEXT("TextureSet");
}

const TCHAR* TextureSetDerivedDataPlugin::GetVersionString() const
{
	return TEXT("B022E127-9B3D-4D58-9B24-1C7AA5C6AF3B");
}

FString TextureSetDerivedDataPlugin::GetPluginSpecificCacheKeySuffix() const
{
	return Cooker->TextureSetDataId.ToString();
}

bool TextureSetDerivedDataPlugin::IsBuildThreadsafe() const
{
	return false; // TODO: Enable this if possible
}

bool TextureSetDerivedDataPlugin::IsDeterministic() const
{
	return false; // TODO: Enable this if possible
}

FString TextureSetDerivedDataPlugin::GetDebugContextString() const
{
	return Cooker->TextureSet->GetFullName();
}

bool TextureSetDerivedDataPlugin::Build(TArray<uint8>& OutData)
{
	Cooker->Build();

	OutData.Empty(2048);
	FMemoryWriter MemWriterAr(OutData);
	Cooker->TextureSet->GetDerivedData()->Serialize(MemWriterAr);

	return true;
}

TextureSetCooker::TextureSetCooker(UTextureSet* TS)
	: TextureSet(TS)
	, Context(TS)
	, Graph(FTextureSetProcessingGraph(TS->Definition->GetModules()))
	, ModuleInfo(TS->Definition->GetModuleInfo())
	, PackingInfo(TS->Definition->GetPackingInfo())
	, AsyncTask(nullptr)
{
	check(IsValid(TextureSet->Definition));
	check(IsInGameThread());

	TextureSetDataId = TS->ComputeTextureSetDataId();

	for (int i = 0; i < PackingInfo.NumPackedTextures(); i++)
		PackedTextureIds.Add(TS->ComputePackedTextureDataID(i));

	// Derived textures must be configured before cooking, so PlatformDataExistsInCache() uses the correct keys
	for (int t = 0; t < TextureSet->DerivedTextures.Num(); t++)
		ConfigureTexture(t);
}

bool TextureSetCooker::CookRequired() const
{
	check(IsInGameThread());

	if (TextureSetDataId != TextureSet->DerivedData->Id)
		return true;

	for (int t = 0; t < TextureSet->DerivedTextures.Num(); t++)
	{
		// Make sure the texture is using the right key before checking if it's cached
		ConfigureTexture(t);

		if (!TextureSet->DerivedTextures[t]->PlatformDataExistsInCache())
			return true;
	}

	return false;
}

void TextureSetCooker::ConfigureTexture(int Index) const
{
	// Configure texture MUST be called only in the game thread.
	// Changing texture properties in a thread can cause a crash in the texture compiler.
	check(IsInGameThread());

	const FTextureSetPackedTextureDef TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = PackingInfo.GetPackedTextureInfo(Index);

	UTexture* Texture = TextureSet->GetDerivedTexture(Index);

	// sRGB if possible
	Texture->SRGB = TextureInfo.HardwareSRGB;

	// Make sure the texture's compression settings are correct
	Texture->CompressionSettings = TextureDef.CompressionSettings;

	// Let the texture compression know if we don't need the alpha channel
	Texture->CompressionNoAlpha = TextureInfo.ChannelCount <= 3;

	// Set the ID of the generated source to match the hash ID of this texture.
	// This will be used to recover the cooked texture data from the DDC if possible.
	Texture->Source.SetId(PackedTextureIds[Index], true);
	Texture->bSourceBulkDataTransient = true;
}

void TextureSetCooker::Execute()
{
	check(!IsAsyncJobInProgress());
	check(IsInGameThread());

	ExecuteInternal();
}

void TextureSetCooker::ExecuteAsync(FQueuedThreadPool* InQueuedPool, EQueuedWorkPriority InQueuedWorkPriority)
{
	check(!IsAsyncJobInProgress());
	check(IsInGameThread());

	AsyncTask = MakeUnique<FAsyncTask<FTextureSetCookerTaskWorker>>(this);
	AsyncTask->StartBackgroundTask(InQueuedPool, InQueuedWorkPriority);
}

void TextureSetCooker::Finalize()
{
	check(IsInGameThread());
	check(!IsAsyncJobInProgress())

	for (int t = 0; t < TextureSet->DerivedTextures.Num(); t++)
	{
		// Configure textures is called again after cooking, to make sure the correct keys are used.
		// If Mip data was updated in BuildTextureData, it would reset the key incorrectly.
		ConfigureTexture(t);

		// Update resources re-caches the derived texture data
		TextureSet->DerivedTextures[t]->UpdateResource();
		TextureSet->DerivedTextures[t]->UpdateCachedLODBias();
	}


}

bool TextureSetCooker::IsAsyncJobInProgress()
{
	check(IsInGameThread());
	return AsyncTask.IsValid() && !AsyncTask->IsDone();
}

bool TextureSetCooker::TryCancel()
{
	check(IsInGameThread());
	if (IsAsyncJobInProgress())
		return AsyncTask->Cancel();
	else
		return true;
}

void TextureSetCooker::ExecuteInternal()
{
	check(TextureSet->ActiveCooker.Get() == this);

	bool bDataWasBuilt = false;

	// Fetch or build derived data for the texture set
	FGuid NewID = TextureSet->ComputeTextureSetDataId();
	if (NewID != TextureSet->DerivedData->Id)
	{
		// Keys to current derived data don't match
		// Retreive derived data from the DDC, or cook new data
		TArray<uint8> Data;
		FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();
		//COOK_STAT(auto Timer = NavCollisionCookStats::UsageStats.TimeSyncWork());
		if (DDC.GetSynchronous(new TextureSetDerivedDataPlugin(this), Data, &bDataWasBuilt))
		{
			//COOK_STAT(Timer.AddHitOrMiss(bDataWasBuilt ? FCookStats::CallStats::EHitOrMiss::Miss : FCookStats::CallStats::EHitOrMiss::Hit, OutData.Num()));
			if (!bDataWasBuilt)
			{
				// If data was not built, it was retreived from the cache; de-serialized it.
				FMemoryReader MemoryReaderDerivedData(Data);
				TextureSet->DerivedData->Serialize(MemoryReaderDerivedData);
			}
		}
	}

	// Ensure our textures are up to date
	if (!bDataWasBuilt)
	{
		for (int t = 0; t < TextureSet->DerivedTextures.Num(); t++)
		{
			// This can happen if a texture build gets interrupted after a texture-set cook.
			if (!TextureSet->DerivedTextures[t]->PlatformDataExistsInCache())
			{
				// Build this texture to regenerate the source data, so it can be cached.
				BuildTextureData(t);
			}
		}
	}
}

void TextureSetCooker::Build() const
{
	TObjectPtr<UTextureSetDerivedData> DerivedData = TextureSet->DerivedData;

	check(IsValid(DerivedData))

	// Update our derived data key
	DerivedData->Id = TextureSetDataId;

	DerivedData->PackedTextureData.SetNum(PackingInfo.NumPackedTextures());

	const EParallelForFlags ParallelForFlags = CVarTextureSetParallelCook.GetValueOnAnyThread() ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread;
	
	ParallelFor(DerivedData->PackedTextureData.Num(), [this](int32 t)
	{
		BuildTextureData(t);

	}, ParallelForFlags);
}

void TextureSetCooker::BuildTextureData(int Index) const
{
	FPackedTextureData& Data = TextureSet->DerivedData->PackedTextureData[Index];

	Data.MaterialParameters.Empty();
	Data.Id = PackedTextureIds[Index];

	const FTextureSetPackedTextureDef TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = PackingInfo.GetPackedTextureInfo(Index);
	int Width = 4;
	int Height = 4;
	float Ratio = 0;

	const TMap<FName, TSharedRef<IProcessingNode>>& OutputTextures = Graph.GetOutputTextures();

	for (int c = 0; c < TextureInfo.ChannelCount; c++)
	{
		const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

		if (!OutputTextures.Contains(ChanelInfo.ProcessedTexture))
			continue;

		const TSharedRef<IProcessingNode> OutputTexture = OutputTextures.FindChecked(ChanelInfo.ProcessedTexture);

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
	UTexture* Texture = TextureSet->GetDerivedTexture(Index);
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
			TSharedRef<IProcessingNode> ProcessedTexture = OutputTextures.FindChecked(ChanelInfo.ProcessedTexture);

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
		FVector4 RestoreMul = FVector4::One();
		FVector4 RestoreAdd = FVector4::Zero();

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

		if (RestoreMul != FVector4::One() || RestoreAdd != FVector4::Zero())
		{
			Data.MaterialParameters.Add(TextureInfo.RangeCompressMulName, RestoreMul);
			Data.MaterialParameters.Add(TextureInfo.RangeCompressAddName, RestoreAdd);
		}
	}

	Texture->Source.UnlockMip(0);
}

#endif