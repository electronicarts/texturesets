// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetCooker.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetModule.h"
#include "Textures/TextureWrapper.h"
#include "Textures/DefaultTexture.h"
#include "Textures/TextureOperatorEnlarge.h"
#include "TextureSetDerivedData.h"
#if WITH_EDITOR
#include "DerivedDataBuildVersion.h"
#endif

static TAutoConsoleVariable<int32> CVarTextureSetParallelCook(
	TEXT("r.TextureSet.ParallelCook"),
	1,
	TEXT("Execute the texture cooking across multiple threads in parallel when possible"),
	ECVF_Default);

int GetPixelIndex(int X, int Y, int Channel, int Width, int Height)
{
	return Y * Width * 4
		+ X * 4
		+ Channel;
}

TextureSetDerivedDataPlugin::TextureSetDerivedDataPlugin(TSharedRef<TextureSetCooker> CookerRef)
	: Cooker(CookerRef)
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
	return Cooker->TextureSetDataKey;
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
	if (Cooker->IsOutOfDate())
		return false; // TODO: What are the consequences of this? Do we fail gracefully?

	Cooker->Build();

	OutData.Empty(2048);
	FMemoryWriter MemWriterAr(OutData);
	Cooker->TextureSet->GetDerivedData()->Serialize(MemWriterAr);

	return true;
}

TextureSetCooker::TextureSetCooker(UTextureSet* TS)
	: ModuleInfo(TS->Definition->GetModuleInfo())
	, PackingInfo(TS->Definition->GetPackingInfo())
{
	TextureSet = TS;
	Definition = TS->Definition;

	TextureSetDataKey = TS->ComputeTextureSetDataKey();

	for (int i = 0; i < PackingInfo.NumPackedTextures(); i++)
		PackedTextureKeys.Add(TS->ComputePackedTextureKey(i));

	// Fill in source textures so the modules can define processing
	for (FTextureSetSourceTextureDef SourceTextureDef : ModuleInfo.GetSourceTextures())
	{
		const TObjectPtr<UTexture>* SourceTexturePtr = TextureSet->SourceTextures.Find(SourceTextureDef.Name);
		if (SourceTexturePtr && SourceTexturePtr->Get())
		{
			TSharedRef<FImageWrapper> ImageWrapper = MakeShared<FImageWrapper>(SourceTexturePtr->Get());
			Context.SourceTextures.Add(SourceTextureDef.Name, ImageWrapper);
		}
		else
		{
			// Sub in a default value if we don't have a source in our texture-set
			TSharedRef<FDefaultTexture> DefaultTexture = MakeShared<FDefaultTexture>(SourceTextureDef.DefaultValue, SourceTextureDef.ChannelCount);
			Context.SourceTextures.Add( SourceTextureDef.Name, DefaultTexture);
		}
	}

	// Modules fill in the processed textures
	for (const UTextureSetModule* Module : Definition->GetModules())
	{
		Module->Process(Context);
	}
}

bool TextureSetCooker::IsOutOfDate() const
{
	return TextureSet->ComputeTextureSetDataKey() != TextureSetDataKey;
}

bool TextureSetCooker::IsOutOfDate(int PackedTextureIndex) const
{
	if (PackedTextureIndex >= PackedTextureKeys.Num())
		return true;

	return TextureSet->ComputePackedTextureKey(PackedTextureIndex) != PackedTextureKeys[PackedTextureIndex];
}

void TextureSetCooker::Build() const
{
	TObjectPtr<UTextureSetDerivedData> DerivedData = TextureSet->DerivedData;

	check(IsValid(DerivedData))

	// Update our derived data key
	DerivedData->Key = TextureSetDataKey;

	DerivedData->PackedTextureData.SetNum(PackingInfo.NumPackedTextures());

	const EParallelForFlags ParallelForFlags = CVarTextureSetParallelCook.GetValueOnAnyThread() ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread;
	
	ParallelFor(DerivedData->PackedTextureData.Num(), [this](int32 t)
	{
		BuildTextureData(t);

	}, ParallelForFlags);
}

void TextureSetCooker::BuildTextureData(int Index) const
{
	check(!IsOutOfDate(Index));

	FPackedTextureData& Data = TextureSet->DerivedData->PackedTextureData[Index];

	Data.MaterialParameters.Empty();
	Data.Key = PackedTextureKeys[Index];

	UE::DerivedData::FBuildVersionBuilder IdBuilder;
	IdBuilder << Data.Key;
	// Setting a new ID should cause the texture's platform data to be invalidated due to the user data we attached to it.
	Data.Id = IdBuilder.Build();

	const FTextureSetPackedTextureDef TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = PackingInfo.GetPackedTextureInfo(Index);
	int Width = 4;
	int Height = 4;
	float Ratio = 0;

	for (int c = 0; c < TextureInfo.ChannelCount; c++)
	{
		const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

		if (!Context.ProcessedTextures.Contains(ChanelInfo.ProcessedTexture))
			continue;

		const TSharedRef<ITextureSetTexture> ProcessedTexture = Context.ProcessedTextures.FindChecked(ChanelInfo.ProcessedTexture);

		ProcessedTexture->Initialize();

		const int ChannelWidth = ProcessedTexture->GetWidth();
		const int ChannelHeight = ProcessedTexture->GetHeight();
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

		if (c < TextureInfo.ChannelCount && Context.ProcessedTextures.Contains(ChanelInfo.ProcessedTexture))
		{
			TSharedRef<ITextureSetTexture> ProcessedTexture = Context.ProcessedTextures.FindChecked(ChanelInfo.ProcessedTexture);

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

	// Range compression
	if (TextureDef.bDoRangeCompression)
	{
		FVector4 RestoreMul = FVector4::One();
		FVector4 RestoreAdd = FVector4::Zero();

		for (int c = 0; c < TextureInfo.ChannelCount; c++)
		{
			if (TextureInfo.ChannelInfo[c].ChannelEncoding != ETextureSetTextureChannelEncoding::Linear_RangeCompressed)
				continue;

			const float Min = MinPixelValues[c];
			const float Max = MaxPixelValues[c];

			if (Min >= Max) // Can happen if the texture is a solid fill
				continue;

			for (int x = 0; x < Width; x++)
			{
				for (int y = 0; y < Height; y++)
				{
					int PixelIndex = GetPixelIndex(x, y, c, Width, Height);

					PixelValues[PixelIndex] = (PixelValues[PixelIndex] - Min) * Max - Min;
				}
			}

			RestoreMul[c] = 1.0f / (Max - Min);
			RestoreAdd[c] = Min;
		}
		Data.MaterialParameters.Add(TextureInfo.RangeCompressMulName, RestoreMul);
		Data.MaterialParameters.Add(TextureInfo.RangeCompressAddName, RestoreAdd);
	}

	Texture->Source.UnlockMip(0);

	UpdateTexture(Index);
}

void TextureSetCooker::UpdateTexture(int Index) const
{
	const FTextureSetPackedTextureDef TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = PackingInfo.GetPackedTextureInfo(Index);
	const FPackedTextureData& Data = TextureSet->DerivedData->PackedTextureData[Index];

	UTexture* Texture = TextureSet->GetDerivedTexture(Index);

	// sRGB if possible
	Texture->SRGB = TextureInfo.HardwareSRGB;

	// Make sure the texture's compression settings are correct
	Texture->CompressionSettings = TextureDef.CompressionSettings;

	// Let the texture compression know if we don't need the alpha channel
	Texture->CompressionNoAlpha = TextureInfo.ChannelCount <= 3;

	// Set the ID of the generated source to match the hash ID of this texture.
	// This will be used to recover the cooked texture data from the DDC if possible.
	Texture->Source.SetId(Data.Id, true);
	Texture->bSourceBulkDataTransient = true;
}
