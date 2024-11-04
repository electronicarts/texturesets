// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetAssetParams.h"
#include "TextureSetSourceTextureReference.h"

struct FTextureSetProcessingContext
{
	TMap<FName, FTextureSetSourceTextureReference> SourceTextures;
	FTextureSetAssetParamsCollection AssetParams;
};
