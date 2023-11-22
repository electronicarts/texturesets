// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetsHelpers.generated.h"

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

#if WITH_EDITOR
	static bool GetSourceDataIdAsString(const UTexture* Texture, FString& StringOut);
#endif
	static bool GetSourceDataIdAsString(const FAssetData& AssetData, FString& StringOut);

	static TArray<FName> GetUnpackedChannelNames(const TArray<FTextureSetPackedTextureDef>& PackedTextures, const TMap<FName, FTextureSetProcessedTextureDef>& ProcessedTextures);
};

UCLASS()
class TEXTURESETS_API UReferenceHolder : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TMap<FGuid, UObject*> References;

};

// Utility class for holding references of UObjects to prevent them from being garbage collected.
class FReferenceHolder
{
public:
	FReferenceHolder() {}

	FReferenceHolder(TArray<UObject*> Objects)
	{
		for (UObject* Object : Objects)
		{
			FGuid Id = FGuid::NewGuid();
			Ids.Add(Id);
			GetHolder()->References.Add(Id, Object);
		}
	}

	~FReferenceHolder()
	{
		for (FGuid Id : Ids)
		{
			GetHolder()->References.Remove(Id);
		}
	}

private:
	TArray<FGuid> Ids;

	static UReferenceHolder* GetHolder();
};
