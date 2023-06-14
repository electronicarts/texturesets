// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetCooker.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetModule.h"
#include "TextureSetModifiersAssetUserData.h"
#include "Textures/ImageWrapper.h"
#include "Textures/DefaultTexture.h"

int GetPixelIndex(int X, int Y, int Channel, int Width, int Height)
{
	return Y * Width * 4
		+ X * 4
		+ Channel;
}

void TextureSetCooker::Prepare()
{
	check(!IsPrepared);

	const UTextureSetDefinition* Definition = TextureSet->Definition;
	SharedInfo = Definition->GetSharedInfo();
	PackingInfo = Definition->GetPackingInfo();

	// Fill in source textures so the modules can define processing
	// TODO: Execute in parallel for
	for (TextureSetTextureDef SourceTextureDef : SharedInfo.GetSourceTextures())
	{
		const TObjectPtr<UTexture>* SourceTexturePtr = TextureSet->SourceTextures.Find(SourceTextureDef.Name);
		if (SourceTexturePtr && SourceTexturePtr->Get())
		{
			// Decode mips and convert to linear for processing
			UTexture* Tex = SourceTexturePtr->Get();
			FImage Image;
			Tex->Source.GetMipImage(Image, 0, 0, 0); // TODO: Make sure this works
			TSharedRef<FImageWrapper> ImageWrapper = MakeShared<FImageWrapper>(Image, (int)Tex->SourceColorSettings.EncodingOverride);
			Context.SourceTextures.Add(SourceTextureDef.Name, ImageWrapper);
		}
		else
		{
			// Sub in a default value if we don't have a source in our texture-set
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

	IsPrepared = true;
}

void TextureSetCooker::PackTexture(int Index, TMap<FName, FVector4>& MaterialParams) const
{
	check(IsPrepared);

	const FTextureSetPackedTextureDef TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const TextureSetPackingInfo::TextureSetPackedTextureInfo TextureInfo = PackingInfo.GetPackedTextureInfo(Index);
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

	UTexture2D* PackedTexture = CastChecked<UTexture2D>(TextureSet->GetCookedTexture(Index));
	PackedTexture->Source.Init(Width, Height, 1, 1, TSF_RGBA16F);

	FFloat16* PixelsValues = (FFloat16*)PackedTexture->Source.LockMip(0);

	float MaxPixelValues[4] {};
	float MinPixelValues[4] {};

	// Copy processed textures into packed textures
	// TODO: Execute each channel in ParallelFor?
	for (int c = 0; c < TextureInfo.ChannelCount; c++)
	{
		const auto& ChanelInfo = TextureInfo.ChannelInfo[c];
		TSharedRef<ITextureSetTexture> ProcessedTexture = Context.ProcessedTextures.FindChecked(ChanelInfo.ProcessedTexture);

		// TODO: Resample textures if sizes dont match
		//check(ProcessedTexture->GetWidth() == Width);
		//check(ProcessedTexture->GetHeight() == Height);

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

	// sRGB if possible
	PackedTexture->SRGB = TextureDef.GetHardwareSRGBEnabled() && TextureInfo.AllowHardwareSRGB;

	// Range compression
	// Note: Range compression is not compatible with sRGB, and sRGB is preferred.
	if (TextureDef.bDoRangeCompression && !PackedTexture->SRGB)
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

		MaterialParams.Add(TextureInfo.RangeCompressMulName, RestoreMul);
		MaterialParams.Add(TextureInfo.RangeCompressAddName, RestoreAdd);
	}

	PackedTexture->Source.UnlockMip(0);
	PackedTexture->Modify(true);
}

void TextureSetCooker::PackAllTextures(TMap<FName, FVector4>& MaterialParams) const
{
	for (int i = 0; i < PackingInfo.NumPackedTextures(); i++)
	{
		PackTexture(i, MaterialParams);
	}
}
