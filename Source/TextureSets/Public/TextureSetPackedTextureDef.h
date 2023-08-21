// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TextureSetPackedTextureDef.generated.h"

uint32 GetTypeHash(const struct FTextureSetPackedTextureDef& Value);

USTRUCT(BlueprintType)
struct TEXTURESETS_API FTextureSetPackedTextureDef
{
	GENERATED_BODY()
public:
	// Texture Format
	UPROPERTY(EditAnywhere)
	TEnumAsByte<TextureCompressionSettings> CompressionSettings; // Not implemented

	// Sources
	UPROPERTY(EditAnywhere, meta=(GetOptions="EditGetUnpackedChannelNames"))
	FName SourceR;

	UPROPERTY(EditAnywhere, meta=(GetOptions="EditGetUnpackedChannelNames"))
	FName SourceG;

	UPROPERTY(EditAnywhere, meta=(GetOptions="EditGetUnpackedChannelNames"))
	FName SourceB;

	UPROPERTY(EditAnywhere, meta=(GetOptions="EditGetUnpackedChannelNames"))
	FName SourceA;

	UPROPERTY(EditAnywhere)
	int SkipMip; // Not implemented

	// Number of mip-levels that can be streamed. -1 means all mips can stream.
	UPROPERTY(EditAnywhere)
	int32 NumStreamedMips; // Not implemented

	UPROPERTY(EditAnywhere)
	ETextureSamplerFilter Filter; // Not implemented

	UPROPERTY(EditAnywhere)
	bool bDoRangeCompression; // Not implemented

	// Attempt to use hardware sRGB decoding if possible
	UPROPERTY(EditAnywhere)
	bool bHardwareSRGB;

	// How many channels our chosen compressed texture supports
	int AvailableChannels() const;

	// How many channels we actually need. Always less than or equal to AvailableChannels()
	int UsedChannels() const;

	TArray<FName> GetSources() const;

	TArray<FString> GetSourcesWithoutChannel(bool RemoveDuplicate = true) const;

	bool GetHardwareSRGBEnabled() const;
};

inline uint32 GetTypeHash(const FTextureSetPackedTextureDef& Value)
{
	uint32 Hash = 0;

	Hash = HashCombine(Hash, GetTypeHash(Value.SourceR));
	Hash = HashCombine(Hash, GetTypeHash(Value.SourceG));
	Hash = HashCombine(Hash, GetTypeHash(Value.SourceB));
	Hash = HashCombine(Hash, GetTypeHash(Value.SourceA));
	Hash = HashCombine(Hash, GetTypeHash(Value.SkipMip));
	Hash = HashCombine(Hash, GetTypeHash(Value.NumStreamedMips));
	Hash = HashCombine(Hash, GetTypeHash((int)Value.Filter));
	Hash = HashCombine(Hash, GetTypeHash(Value.bDoRangeCompression));
	Hash = HashCombine(Hash, GetTypeHash(Value.bHardwareSRGB));

	return Hash;
}