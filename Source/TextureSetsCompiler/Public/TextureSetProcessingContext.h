// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "TextureSetAssetParams.h"
#include "TextureSetSourceTextureReference.h"

struct FTextureSetProcessingContext
{
	TMap<FName, FTextureSetSourceTextureReference> SourceTextures;
	FTextureSetAssetParamsCollection AssetParams;
};
#endif
