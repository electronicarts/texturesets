// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetProcessingContext.h"
#include "TextureSet.h"

FTextureSetProcessingContext::FTextureSetProcessingContext(const class UTextureSet* TextureSet)
	: SourceTextures(TextureSet->SourceTextures)
{
}