// (c) Electronic Arts. All Rights Reserved.

#if WITH_EDITOR

#include "ProcessingNodes/TextureRead.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "TextureSetDefinition.h"
#include "TextureSetProcessingGraph.h"
#include "TextureSetsHelpers.h"

FTextureRead::FTextureRead(FName SourceNameIn, const FTextureSetSourceTextureDef& SourceDefinitionIn)
	: SourceName(SourceNameIn)
	, SourceDefinition(SourceDefinitionIn)
	, Texture(nullptr)
	, bLoaded(false)
	, bInitialized(false)
	, ValidChannels(0)
	, ChannelSwizzle{0, 1, 2, 3}
	, Width(1)
	, Height(1)
	, Slices(1)
	, bValidImage(false)
{
}

void FTextureRead::LoadResources(const FTextureSetProcessingContext& Context)
{
#if TS_SOFT_SOURCE_TEXTURE_REF
	check(!IsLoading()); // LoadSynchronous often fails if we are loading something else
#endif

	if (bLoaded)
		return;

	if(!IsValid(Texture) && Context.HasSourceTexure(SourceName))
	{
		const FTextureSetSourceTextureReference& TextureRef = Context.GetSourceTexture(SourceName);
		Texture = TextureRef.GetTexture();
		ChannelMask = TextureRef.ChannelMask;

		if(IsValid(Texture))
		{
			TextureReferenceHolder = FReferenceHolder({Texture});

			Width = Texture->Source.GetSizeX();
			Height = Texture->Source.GetSizeY();
			Slices = Texture->Source.GetNumSlices();

			switch (Texture->Source.GetFormat())
			{
			case ETextureSourceFormat::TSF_G8:
			case ETextureSourceFormat::TSF_G16:
			case ETextureSourceFormat::TSF_R16F:
			case ETextureSourceFormat::TSF_R32F:
				ValidChannels = 1;
				break;
			case ETextureSourceFormat::TSF_BGRA8:
			case ETextureSourceFormat::TSF_BGRE8:
			case ETextureSourceFormat::TSF_RGBA16:
			case ETextureSourceFormat::TSF_RGBA16F:
			case ETextureSourceFormat::TSF_RGBA32F:
				ValidChannels = 4;
				break;
			default:
				unimplemented();
			}

			uint8 CurrentChannel = 0;

			// Start at the first valid, unmasked channel
			while((CurrentChannel + 1 < ValidChannels) && !(ChannelMask & (1 << CurrentChannel)))
				CurrentChannel++;

			for (int i = 0; i < SourceDefinition.ChannelCount; i++)
			{
				ChannelSwizzle[i] = CurrentChannel;

				// Advance to the next valid, unmasked channel
				for (uint8 NextChannel = CurrentChannel + 1; NextChannel < ValidChannels; NextChannel++)
				{
					if (ChannelMask & (1 << NextChannel))
					{
						CurrentChannel = NextChannel;
						break;
					}
				}
			}
		}
	}

	bLoaded = true;
}

void FTextureRead::Initialize(const FTextureSetProcessingGraph& Graph)
{
	FScopeLock Lock(&InitializeCS);

	if (bInitialized)
		return;

	if(IsValid(Texture))
	{
		check(Texture->Source.IsValid());

		// This does two copies, and ends up being a bottleneck in the cook
		// TODO: Read the mip data without needing to do a copy every time
		FImage RawImage;
		bValidImage = Texture->Source.GetMipImage(RawImage, 0, 0, 0);
		RawImage.Linearize((int)Texture->SourceColorSettings.EncodingOverride, Image);
	}

	bInitialized = true;
}

const uint32 FTextureRead::ComputeGraphHash() const
{
	uint32 Hash = GetTypeHash(GetNodeTypeName().ToString());

	Hash = HashCombine(Hash, GetTypeHash(SourceName.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(SourceDefinition));

	return Hash;
}

const uint32 FTextureRead::ComputeDataHash(const FTextureSetProcessingContext& Context) const
{
#if TS_SOFT_SOURCE_TEXTURE_REF
	check(!IsLoading()); // LoadSynchronous often fails if we are loading something else
#endif

	uint32 Hash = 0;

	if(Context.HasSourceTexure(SourceName))
	{
		const FTextureSetSourceTextureReference& TextureRef = Context.GetSourceTexture(SourceName);

		if (!TextureRef.IsNull())
		{
			FString PayloadIdString;

			// If referenced texture is already loaded just grab the ID string from it directly
			if (!TextureRef.Valid() || !TextureSetsHelpers::GetSourceDataIdAsString(TextureRef.Texture.Get(), PayloadIdString))
			{
#if TS_SOFT_SOURCE_TEXTURE_REF
				// If the referenced texture is not loaded, check if we have the IdString saved with it's asset data
				// so we can avoid loading the texture just to check if it's changed.
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(TextureRef.GetTexturePath());

				if(!TextureSetsHelpers::GetSourceDataIdAsString(AssetData, PayloadIdString))
#endif
				{
					// If we were not able to get the ID string from the asset data, we need to load the texture to retreive it.
					// This should only happen on textures that were created before the texture-sets plugin was enabled, and haven't been re-saved since.
					// As existing textures are modified and re-saved, this should happen less frequently.
					UTexture* T = TextureRef.GetTexture();
					check(IsValid(T));
					FGuid PersistentID = T->Source.GetPersistentId();
					TextureSetsHelpers::GetSourceDataIdAsString(T, PayloadIdString);
				}
			}
			
			check(!PayloadIdString.IsEmpty())
			Hash = HashCombine(Hash, GetTypeHash(PayloadIdString));
			Hash = HashCombine(Hash, GetTypeHash(TextureRef.ChannelMask));
		}
	}

	return Hash;
}

#if PROCESSING_METHOD == PROCESSING_METHOD_CHUNK
void FTextureRead::ComputeChunk(const FTextureProcessingChunk& Chunk, float* TextureData) const
{
	if (bValidImage)
	{
		const float* RawImageData = (float*)Image.RawData.GetData();
		constexpr int RawImagePixelStride = 4;

		int DataIndex = Chunk.DataStart;
		int PixelIndex = (Chunk.FirstPixel * RawImagePixelStride) + Chunk.Channel;
		for (; DataIndex <= Chunk.DataEnd; DataIndex += Chunk.DataPixelStride, PixelIndex += RawImagePixelStride)
		{
			TextureData[DataIndex] = RawImageData[PixelIndex];
		}
	}
	else
	{
		// Fill tile data with default value
		for (int DataIndex = Chunk.DataStart; DataIndex <= Chunk.DataEnd; DataIndex += Chunk.DataPixelStride)
		{
			TextureData[DataIndex] = SourceDefinition.DefaultValue[Chunk.Channel];
		}
	}
}
#else
float FTextureRead::GetPixel(int X, int Y, int Z, int Channel) const
{
#ifdef UE_BUILD_DEBUG
	// Too slow to be used in development builds
	check(bInitialized);
	check(Channel < SourceDefinition.ChannelCount);
#endif

	// TODO: Can we avoid a branch on every pixel?
	if (bValidImage)
	{
		const int I = Z * Image.GetWidth() * Image.GetHeight() + Y * Image.GetWidth() + X;
#ifdef UE_BUILD_DEBUG
		check(X < Image.GetWidth());
		check(Y < Image.GetHeight());
		check(Z < Image.NumSlices);
		check(I < Image.GetNumPixels());
#endif
		return Image.AsRGBA32F()[I].Component(ChannelSwizzle[Channel]);
	}
	else
	{
		return SourceDefinition.DefaultValue[Channel];
	}
}
#endif
#endif
