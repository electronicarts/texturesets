// (c) Electronic Arts. All Rights Reserved.

#if WITH_EDITOR

#include "ProcessingNodes/TextureInput.h"
#include "TextureSetProcessingGraph.h"
#include "TextureSetDefinition.h"

FTextureInput::FTextureInput(FName SourceNameIn, const FTextureSetSourceTextureDef& SourceDefinitionIn)
{
	SourceName = SourceNameIn;
	SourceDefinition = SourceDefinitionIn;
	bInitialized = false;
	ValidChannels = 0;
}

void FTextureInput::Initialize(const FTextureSetProcessingContext& Context)
{
	FScopeLock Lock(&InitializeCS);

	if (bInitialized)
		return;

	if(Context.HasSourceTexure(SourceName))
	{
		UTexture* Texture = Context.GetSourceTexture(SourceName);
		
		if (IsValid(Texture))
		{
			FImage RawImage;
			Texture->Source.GetMipImage(RawImage, 0, 0, 0);

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
		UTexture* Texture = Context.GetSourceTexture(SourceName);
		if (IsValid(Texture))
		{
			Hash = HashCombine(Hash, GetTypeHash(Texture->Source.GetId()));
		}
	}

	return Hash;
}

float FTextureInput::GetPixel(int X, int Y, int Channel) const
{
	check(bInitialized);
	check(Channel < SourceDefinition.ChannelCount);

	// TODO: Can we avoid a branch on every pixel?
	if (Channel < ValidChannels)
	{
		const int I = Y * Image.GetWidth() + X;
		return Image.AsRGBA32F()[I].Component(Channel);
	}
	else
	{
		return SourceDefinition.DefaultValue[Channel];
	}
}
#endif
