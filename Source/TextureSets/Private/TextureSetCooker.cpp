// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetCooker.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetModule.h"
#include "TextureSetInfo.h"
#include "TextureSetModifiersAssetUserData.h"
#include "Textures/ImageWrapper.h"
#include "Textures/DefaultTexture.h"

int GetPixelIndex(int X, int Y, int Channel, int Width, int Height)
{
	return Y * Width * 4
		+ X * 4
		+ Channel;
}

void TextureSetCooker::CookTextureSet(UTextureSet* TextureSet)
{
	const UTextureSetDefinition* Definition = TextureSet->Definition;
	const TextureSetDefinitionSharedInfo SharedInfo = Definition->GetSharedInfo();

	FTextureSetProcessingContext Context;

	// Fill in source textures so the modules can define processing
	for (TextureSetTextureDef SourceTextureDef : SharedInfo.GetSourceTextures())
	{
		if (TextureSet->SourceTextures.Contains(SourceTextureDef.Name))
		{
			UTexture* Tex = TextureSet->SourceTextures.FindChecked(SourceTextureDef.Name);
			TSharedRef<FImage> Image = MakeShared<FImage>();
			Tex->Source.GetMipImage(*Image, 0, 0, 0); // TODO: Make sure this works
			Context.SourceTextures.Add(SourceTextureDef.Name, MakeShared<FImageWrapper>(Image));
		}
		else
		{
			Context.SourceTextures.Add(
				SourceTextureDef.Name,
				MakeShared<FDefaultTexture>(SourceTextureDef.DefaultValue, SourceTextureDef.ChannelCount));
		}
	}

	// Modules fill in the processed textures
	for (const UTextureSetModule* Module : Definition->GetModules())
	{
		Module->Process(Context);
	}

	const TextureSetPackingInfo PackingInfo = Definition->GetPackingInfo();

	// Garbage collection will destroy the unused cooked textures when all references from material instance are removed
	TextureSet->CookedTextures.SetNum(PackingInfo.NumPackedTextures());
	TextureSet->ShaderParameters.Empty();

	// Generate packed textures
	for (int t = 0; t < PackingInfo.NumPackedTextures(); t++)
	{
		const FTextureSetPackedTextureDef TextureDef = PackingInfo.GetPackedTextureDef(t);
		const TextureSetPackingInfo::TextureSetPackedTextureInfo& TextureInfo = PackingInfo.GetPackedTextureInfo(t);
		int Width = 0;
		int Height = 0;
		float Ratio = 0;

		for (int c = 0; c < TextureInfo.ChannelCount; c++)
		{
			const auto& ChanelInfo = TextureInfo.ChannelInfo[c];
			const TSharedRef<ITextureSetTexture> ProcessedTexture = Context.ProcessedTextures.FindChecked(ChanelInfo.ProcessedTexture);
			const int ChannelWidth = ProcessedTexture->GetWidth();
			const int ChannelHeight = ProcessedTexture->GetHeight();
			const float NewRatio = (float)Width / (float)Height;

			// Calculate the maximum size of all of our processed textures. We'll use this as our packed texture size.
			Width = FMath::Max(Width, ChannelWidth);
			Height = FMath::Max(Height, ChannelHeight);
			// Verify that all processed textures have the same aspect ratio
			check(NewRatio == Ratio || Ratio == 0);
			Ratio = NewRatio;
		}

		TArray<FFloat16> PixelsValues;
		PixelsValues.SetNum(Width * Height * 4);

		float MaxPixelValues[4] {};
		float MinPixelValues[4] {};

		// Copy processed textures into packed textures
		for (int c = 0; c < TextureInfo.ChannelCount; c++)
		{
			const auto& ChanelInfo = TextureInfo.ChannelInfo[c];
			TSharedRef<ITextureSetTexture> ProcessedTexture = Context.ProcessedTextures.FindChecked(ChanelInfo.ProcessedTexture);

			// TODO: Resample textures if sizes dont match
			check(ProcessedTexture->GetWidth() == Width);
			check(ProcessedTexture->GetHeight() == Height);

			// Initialize the max and min pixel values so they will be overridden by the first pixel
			MaxPixelValues[c] = TNumericLimits<float>::Lowest();
			MinPixelValues[c] = TNumericLimits<float>::Max();

			for (int x = 0; x < Width; x++)
			{
				for (int y = 0; y < Height; y++)
				{
					int PixelIndex = GetPixelIndex(x, y, c, Width, Height);

					float PixelValue = ProcessedTexture->GetPixel(x, y, c);
					MaxPixelValues[c] = FMath::Max(MaxPixelValues[c], PixelValue);
					MinPixelValues[c] = FMath::Min(MinPixelValues[c], PixelValue);
					PixelsValues[PixelIndex] = PixelValue;
				}
			}
		}
		
		// Fill unused channels with black
		for (int x = 0; x < Width; x++)
		{
			for (int y = 0; y < Height; y++)
			{
				for (int c = TextureInfo.ChannelCount; c < 4; c++)
				{
					int PixelIndex = GetPixelIndex(x, y, c, Width, Height);

					PixelsValues[PixelIndex] = 0.0f;
				}
			}
		}

		// Encode Range compression
		if (TextureDef.bDoRangeCompression)
		{
			FVector4 RestoreMul = FVector4::One();
			FVector4 RestoreAdd = FVector4::Zero();

			for (int c = TextureInfo.ChannelCount; c < 4; c++)
			{
				const float Min = MinPixelValues[c];
				const float Max = MaxPixelValues[c];

				if (Min >= Max) // Can happen if the texture is a solid fill
					continue;

				for (int x = 0; x < Width; x++)
				{
					for (int y = 0; y < Height; y++)
					{
						int PixelIndex = GetPixelIndex(x, y, c, Width, Height);

						PixelsValues[PixelIndex] = (PixelsValues[PixelIndex] - Min) * Max - Min;
					}
				}

				RestoreMul[c] = 1.0f / (Max - Min);
				RestoreAdd[c] = Min;
			}

			TextureSet->ShaderParameters.Add(TextureInfo.RangeCompressMulName, RestoreMul);
			TextureSet->ShaderParameters.Add(TextureInfo.RangeCompressAddName, RestoreAdd);
		}

		FString TextureName = TextureSet->GetName() + "_PACKED_" + FString::FromInt(t);

		// Locate existing packed texture if it already exist
		UTexture2D* PackedTexture = FindObject<UTexture2D>(TextureSet, *TextureName, true);
		if (!PackedTexture)
		{
			// Create a new packed texture if needed
			PackedTexture = NewObject<UTexture2D>(TextureSet, FName(*TextureName), RF_Public | RF_Standalone);
		}

		// Check for existing user data
		UTextureSetModifiersAssetUserData* TextureModifier = PackedTexture->GetAssetUserData<UTextureSetModifiersAssetUserData>();
		if (!TextureModifier)
		{
			// Create new user data if needed
			FString AssetUserDataName = TextureName + "_AssetUserData";
			TextureModifier = NewObject<UTextureSetModifiersAssetUserData>(PackedTexture, FName(*AssetUserDataName), RF_Public | RF_Standalone);
			PackedTexture->AddAssetUserData(TextureModifier);
		}
		// Configure user data to reference this texture set, so it can invalidate the texture when the hash changes
		TextureModifier->TextureSet = TextureSet;
		TextureModifier->PackedTextureDefIndex = t;

		// TODO: SRGB?

		PackedTexture->Source.Init(Width, Height, 1, 1, TSF_RGBA16F);
		TextureSet->CookedTextures[t] = PackedTexture;
	}
}
