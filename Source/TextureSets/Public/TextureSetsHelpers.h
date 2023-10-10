// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture;
struct FAssetData;

class TEXTURESETS_API TextureSetsHelpers
{
public:
	static FName TextureBulkDataIdAssetTagName;

	static bool GetSourceDataIdAsString(const UTexture* Texture, FString& StringOut);
	static bool GetSourceDataIdAsString(const FAssetData& AssetData, FString& StringOut);
};

