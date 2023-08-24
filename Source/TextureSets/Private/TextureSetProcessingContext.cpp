// (c) Electronic Arts. All Rights Reserved.

#if WITH_EDITOR

#include "TextureSetProcessingContext.h"
#include "TextureSet.h"

FTextureSetProcessingContext::FTextureSetProcessingContext(const class UTextureSet* TextureSet)
	: SourceTextures(TextureSet->SourceTextures)
{
}

#endif