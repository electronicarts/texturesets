// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture;
struct FAssetData;
struct FTextureSetPackedTextureDef;
struct FTextureSetProcessedTextureDef;

class TEXTURESETS_API TextureSetsHelpers
{
public:
	// Suffixes used when addressing specific channels of a texture by name
	// Defined as {".r", ".g", ".b", ".a"}
	static const TArray<FString> ChannelSuffixes;

	static FName TextureBulkDataIdAssetTagName;

	static bool GetSourceDataIdAsString(const UTexture* Texture, FString& StringOut);
	static bool GetSourceDataIdAsString(const FAssetData& AssetData, FString& StringOut);

	static TArray<FName> GetUnpackedChannelNames(const TArray<FTextureSetPackedTextureDef>& PackedTextures, const TMap<FName, FTextureSetProcessedTextureDef>& ProcessedTextures);
};

