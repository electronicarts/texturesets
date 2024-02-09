// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetsHelpers.generated.h"

class UTexture;
struct FAssetData;
struct FTextureSetPackedTextureDef;
struct FTextureSetProcessedTextureDef;

class TEXTURESETSCOMMON_API TextureSetsHelpers
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

	static inline FName MakeTextureParameterName(FName ParameterName, int TextureIndex)
	{
		return FName(FString::Format(TEXT("TEXSET_{0}_PACKED_{1}"), {ParameterName.ToString(), FString::FromInt(TextureIndex)}));
	}

	static inline FName MakeConstantParameterName(FName ParameterName, FName ConstantName)
	{
		return FName(FString::Format(TEXT("TEXSET_{0}_{1}"), {ParameterName.ToString(), ConstantName.ToString()}));
	}

	static inline bool IsTextureSetParameterName(FName Name)
	{
		return Name.ToString().StartsWith("TEXSET_", ESearchCase::IgnoreCase);
	}
};

UCLASS()
class TEXTURESETSCOMMON_API UReferenceHolder : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TMap<FGuid, UObject*> References;

};

// Utility class for holding references of UObjects to prevent them from being garbage collected.
class TEXTURESETSCOMMON_API FReferenceHolder
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
