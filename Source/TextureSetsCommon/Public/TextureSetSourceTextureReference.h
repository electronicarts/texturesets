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
	//TSoftObjectPtr<UTexture> Texture;
	TObjectPtr<UTexture> Texture;

	// This is not currently working, as it has issues loading a soft ref in the postload of another asset.
	#define TS_SOFT_SOURCE_TEXTURE_REF 0

	/** Mask used when reading this source texture. Functions similairly to the channel mask node in the material graph, i.e. un-checking channels means they will be ignored and the next unmasked channel will be used instead. Useful for reading from source textures that have already been channel packed. */
	UPROPERTY(EditAnywhere, meta = (Bitmask, BitmaskEnum = "/Script/TextureSets.ETextureSetSourceTextureChannelMask"))
	int32 ChannelMask;

	inline FSoftObjectPath GetTexturePath() const
	{
#if TS_SOFT_SOURCE_TEXTURE_REF
		return Texture.ToSoftObjectPath();
#else
		return Texture.GetPath();
#endif
	}

	inline bool IsNull() const
	{
#if TS_SOFT_SOURCE_TEXTURE_REF
		return Texture.IsNull();
#else
		return !IsValid(Texture);
#endif
	}

	inline bool Valid() const
	{
#if TS_SOFT_SOURCE_TEXTURE_REF
		return Texture.IsValid();
#else
		return IsValid(Texture);
#endif
	}

	inline UTexture* GetTexture() const
	{
#if TS_SOFT_SOURCE_TEXTURE_REF
		return Texture.LoadSynchronous();
#else
		return Texture.Get();
#endif
	}
};