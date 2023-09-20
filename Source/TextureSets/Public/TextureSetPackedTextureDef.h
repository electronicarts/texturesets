// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TextureSetPackedTextureDef.generated.h"

uint32 GetTypeHash(const struct FTextureSetPackedTextureDef& Value);

USTRUCT(BlueprintType)
struct TEXTURESETS_API FTextureSetPackedTextureDef
{
	GENERATED_BODY()
public:
	FTextureSetPackedTextureDef()
		: SkipMip(0)
		, NumStreamedMips(-1)
		, Filter(ETextureSamplerFilter::AnisotropicLinear)
		, bDoRangeCompression(false)
		, bHardwareSRGB(false)
	{}

	// Texture Format
	UPROPERTY(EditAnywhere)
	TEnumAsByte<TextureCompressionSettings> CompressionSettings;

	// Sources
	UPROPERTY(EditAnywhere, meta=(GetOptions="EditGetUnpackedChannelNames", EditCondition = "AvailableChannels > 0"))
	FName SourceR;

	UPROPERTY(EditAnywhere, meta=(GetOptions="EditGetUnpackedChannelNames", EditCondition = "AvailableChannels > 1"))
	FName SourceG;

	UPROPERTY(EditAnywhere, meta=(GetOptions="EditGetUnpackedChannelNames", EditCondition = "AvailableChannels > 2"))
	FName SourceB;

	UPROPERTY(EditAnywhere, meta=(GetOptions="EditGetUnpackedChannelNames", EditCondition = "AvailableChannels > 3"))
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
	int GetAvailableChannels() const;

	// How many channels we actually need. Always less than or equal to AvailableChannels()
	int GetUsedChannels() const;

	TArray<FName> GetSources() const;
	void SetSource(int Index, FName Value);

	TArray<FString> GetSourcesWithoutChannel(bool RemoveDuplicate = true) const;

	bool GetHardwareSRGBEnabled() const;

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
	Hash = HashCombine(Hash, GetTypeHash(Value.SkipMip));
	Hash = HashCombine(Hash, GetTypeHash(Value.NumStreamedMips));
	Hash = HashCombine(Hash, GetTypeHash((int)Value.Filter));
	Hash = HashCombine(Hash, GetTypeHash(Value.bDoRangeCompression));
	Hash = HashCombine(Hash, GetTypeHash(Value.bHardwareSRGB));

	return Hash;
}