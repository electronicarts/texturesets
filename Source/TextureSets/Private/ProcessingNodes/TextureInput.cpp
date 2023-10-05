// (c) Electronic Arts. All Rights Reserved.

#if WITH_EDITOR

#include "ProcessingNodes/TextureInput.h"

#include "TextureSetDefinition.h"
#include "TextureSetProcessingGraph.h"

FTextureInput::FTextureInput(FName SourceNameIn, const FTextureSetSourceTextureDef& SourceDefinitionIn)
	: SourceName(SourceNameIn)
	, SourceDefinition(SourceDefinitionIn)
	, bInitialized(false)
	, ValidChannels(0)
	, ChannelSwizzle{0, 1, 2, 3}
	, bValidImage(false)
{
}

void FTextureInput::Initialize(const FTextureSetProcessingContext& Context)
{
	FScopeLock Lock(&InitializeCS);

	if (bInitialized)
		return;

	if(Context.HasSourceTexure(SourceName))
	{
		const FTextureSetSourceTextureReference& TextureRef = Context.GetSourceTexture(SourceName);
		
		UTexture* Texture = TextureRef.Texture;

		if (IsValid(Texture))
		{
			check(Texture->Source.IsValid());

			FImage RawImage;
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
				while((CurrentChannel + 1 < ValidChannels) && !(TextureRef.ChannelMask & (1 << CurrentChannel)))
					CurrentChannel++;

				for (int i = 0; i < SourceDefinition.ChannelCount; i++)
				{
					ChannelSwizzle[i] = CurrentChannel;

					// Advance to the next valid, unmasked channel
					for (uint8 NextChannel = CurrentChannel + 1; NextChannel < ValidChannels; NextChannel++)
					{
						if (TextureRef.ChannelMask & (1 << NextChannel))
						{
							CurrentChannel = NextChannel;
							break;
						}
					}
				}
			}
		}
	}

	bInitialized = true;
}

const uint32 FTextureInput::ComputeGraphHash() const
{
	uint32 Hash = GetTypeHash(GetNodeTypeName().ToString());

	Hash = HashCombine(Hash, GetTypeHash(SourceName.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(SourceDefinition));

	return Hash;
}

const uint32 FTextureInput::ComputeDataHash(const FTextureSetProcessingContext& Context) const
{
	uint32 Hash = 0;

	if(Context.HasSourceTexure(SourceName))
	{
		const FTextureSetSourceTextureReference& TextureRef = Context.GetSourceTexture(SourceName);
		UTexture* Texture = TextureRef.Texture;
		if (IsValid(Texture))
		{
			Hash = HashCombine(Hash, GetTypeHash(Texture->Source.GetId()));
			Hash = HashCombine(Hash, GetTypeHash(TextureRef.ChannelMask));
		}
	}

	return Hash;
}

float FTextureInput::GetPixel(int X, int Y, int Channel) const
{
#ifdef UE_BUILD_DEBUG
	// Too slow to be used in development builds
	check(bInitialized);
	check(Channel < SourceDefinition.ChannelCount);
#endif

	// TODO: Can we avoid a branch on every pixel?
	if (bValidImage)
	{
#ifdef UE_BUILD_DEBUG
		check(X < Image.GetWidth());
		check(Y < Image.GetHeight());
#endif
		const int I = Y * Image.GetWidth() + X;
		return Image.AsRGBA32F()[I].Component(ChannelSwizzle[Channel]);
	}
	else
	{
		return SourceDefinition.DefaultValue[Channel];
	}
}
#endif
