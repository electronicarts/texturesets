// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TextureSetPackedTextureDef.generated.h"

USTRUCT(BlueprintType)
struct TEXTURESETS_API FTextureSetPackedTextureDef
{
	GENERATED_BODY()
public:
	// Texture Format
	UPROPERTY(EditAnywhere)
	TEnumAsByte<TextureCompressionSettings> CompressionSettings; // Not implemented

	// Sources
	UPROPERTY(EditAnywhere, meta=(GetOptions="GetUnpackedChannelNames"))
	FName SourceR;

	UPROPERTY(EditAnywhere, meta=(GetOptions="GetUnpackedChannelNames"))
	FName SourceG;

	UPROPERTY(EditAnywhere, meta=(GetOptions="GetUnpackedChannelNames"))
	FName SourceB;

	UPROPERTY(EditAnywhere, meta=(GetOptions="GetUnpackedChannelNames"))
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

	bool GetHardwareSRGBEnabled() const;
};
