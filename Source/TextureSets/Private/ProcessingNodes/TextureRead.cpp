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
	, bInitialized(false)
	, ValidChannels(0)
	, ChannelSwizzle{0, 1, 2, 3}
	, bValidImage(false)
{
}

void FTextureRead::LoadResources(const FTextureSetProcessingContext& Context)
{
	if(!IsValid(Texture) && Context.HasSourceTexure(SourceName))
	{
		const FTextureSetSourceTextureReference& TextureRef = Context.GetSourceTexture(SourceName);
		Texture = TextureRef.Texture.LoadSynchronous();
		ChannelMask = TextureRef.ChannelMask;
	}
}

void FTextureRead::Initialize(const FTextureSetProcessingGraph& Graph)
{
	FScopeLock Lock(&InitializeCS);

	if (bInitialized)
		return;

	if(IsValid(Texture))
	{
		FImage RawImage;
		check(Texture->Source.IsValid());
		bValidImage = Texture->Source.GetMipImage(RawImage, 0, 0, 0);

		if (bValidImage)
		{
			switch (RawImage.Format)
			{
			case ERawImageFormat::G8:
			case ERawImageFormat::G16:
			case ERawImageFormat::R16F:
			case ERawImageFormat::R32F:
				ValidChannels = 1;
				break;
			case ERawImageFormat::BGRE8:
			case ERawImageFormat::BGRA8:
			case ERawImageFormat::RGBA16:
			case ERawImageFormat::RGBA16F:
			case ERawImageFormat::RGBA32F:
				ValidChannels = 4;
				break;
			default:
				unimplemented();
			}

			RawImage.Linearize((int)Texture->SourceColorSettings.EncodingOverride, Image);

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
	uint32 Hash = 0;

	if(Context.HasSourceTexure(SourceName))
	{
		const FTextureSetSourceTextureReference& TextureRef = Context.GetSourceTexture(SourceName);

		if (!TextureRef.Texture.IsNull())
		{
			FString PayloadIdString;

			// If referenced texture is already loaded just grab the ID string from it directly
			if (!TextureRef.Texture.IsValid() || !TextureSetsHelpers::GetSourceDataIdAsString(TextureRef.Texture.Get(), PayloadIdString))
			{
				// If the referenced texture is not loaded, check if we have the IdString saved with it's asset data
				// so we can avoid loading the texture just to check if it's changed.
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(TextureRef.Texture.ToSoftObjectPath());

				if(!TextureSetsHelpers::GetSourceDataIdAsString(AssetData, PayloadIdString))
				{
					// If we were not able to get the ID string from the asset data, we need to load the texture to retreive it.
					// This should only happen on textures that were created before the texture-sets plugin was enabled, and haven't been re-saved since.
					// As existing textures are modified and re-saved, this should happen less frequently.
					UTexture* T = TextureRef.Texture.LoadSynchronous();
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
