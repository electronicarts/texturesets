// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine/TextureDefines.h"

#include "TextureSetPackedTextureDef.generated.h"

uint32 GetTypeHash(const struct FTextureSetPackedTextureDef& Value);

USTRUCT(BlueprintType)
struct TEXTURESETSCOMMON_API FTextureSetPackedTextureDef
{
	GENERATED_BODY()
public:
	FTextureSetPackedTextureDef()
	{}

	// Texture Format
	UPROPERTY(EditAnywhere, Category="PackedTextureDefinition")
	TEnumAsByte<TextureCompressionSettings> CompressionSettings = TextureCompressionSettings::TC_Default;

	// Sources
	UPROPERTY(EditAnywhere, Category="PackedTextureDefinition", meta=(GetOptions="EditGetUnpackedChannelNames", EditCondition = "AvailableChannels > 0"))
	FName SourceR;

	UPROPERTY(EditAnywhere, Category="PackedTextureDefinition", meta=(GetOptions="EditGetUnpackedChannelNames", EditCondition = "AvailableChannels > 1"))
	FName SourceG;

	UPROPERTY(EditAnywhere, Category="PackedTextureDefinition", meta=(GetOptions="EditGetUnpackedChannelNames", EditCondition = "AvailableChannels > 2"))
	FName SourceB;

	UPROPERTY(EditAnywhere, Category="PackedTextureDefinition", meta=(GetOptions="EditGetUnpackedChannelNames", EditCondition = "AvailableChannels > 3"))
	FName SourceA;

	UPROPERTY(EditAnywhere, Category="PackedTextureDefinition")
	bool bVirtualTextureStreaming = false;

	UPROPERTY(EditAnywhere, Category="PackedTextureDefinition")
	int32 LODBias = 0;

	// How many channels our chosen compressed texture supports
	int GetAvailableChannels() const;

	// How many channels we actually need. Always less than or equal to AvailableChannels()
	int GetUsedChannels() const;

	TArray<FName> GetSources() const;
	void SetSource(int Index, FName Value);

	TArray<FString> GetSourcesWithoutChannel(bool RemoveDuplicate = true) const;

	bool GetHardwareSRGBSupported() const;

#if WITH_EDITOR
	// Call to update the private "AvailableChannels" so the EditCondition is accurate
	void UpdateAvailableChannels();
#endif

#if WITH_EDITORONLY_DATA
private:
	// Used to determine if a source is enabled or not, as CanEditChange does not let us determine which index a property belongs to. 
	// See https://udn.unrealengine.com/s/feed/0D54z00007Fbih8CAB
	UPROPERTY()
	int AvailableChannels = 0;
#endif
};

inline uint32 GetTypeHash(const FTextureSetPackedTextureDef& Value)
{
	uint32 Hash = 0;

	Hash = HashCombine(Hash, GetTypeHash(Value.SourceR.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(Value.SourceG.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(Value.SourceB.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(Value.SourceA.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(Value.bVirtualTextureStreaming));
	Hash = HashCombine(Hash, GetTypeHash(Value.LODBias));

	return Hash;
}