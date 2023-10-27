// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Texture.h"

#include "TextureSetSourceTextureReference.generated.h"

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ETextureSetSourceTextureChannelMask: uint8
{
	NONE = 0 UMETA(Hidden),
	R = 1 << 0,
	G = 1 << 1,
	B = 1 << 2,
	A = 1 << 3,
};
ENUM_CLASS_FLAGS(ETextureSetSourceTextureChannelMask);

USTRUCT()
struct FTextureSetSourceTextureReference
{
	GENERATED_BODY()

public:
	FTextureSetSourceTextureReference()
		: Texture(nullptr)
		, ChannelMask((int32)(ETextureSetSourceTextureChannelMask::R | ETextureSetSourceTextureChannelMask::G | ETextureSetSourceTextureChannelMask::B | ETextureSetSourceTextureChannelMask::A))
	{}

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UTexture> Texture;

	/** Mask used when reading this source texture. Functions similairly to the channel mask node in the material graph, i.e. un-checking channels means they will be ignored and the next unmasked channel will be used instead. Useful for reading from source textures that have already been channel packed. */
	UPROPERTY(EditAnywhere, meta = (Bitmask, BitmaskEnum = ETextureSetSourceTextureChannelMask))
	int32 ChannelMask;
};