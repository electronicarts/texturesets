// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreUObject.h"
#include "HAL/CriticalSection.h"
#include "TextureSetDerivedData.generated.h"

USTRUCT()
struct FDerivedTextureData
{
	GENERATED_BODY()

	// Material parameters that were generated along with this packed texture
	UPROPERTY(VisibleAnywhere, Category="DerivedTextureData")
	TMap<FName, FVector4f> TextureParameters;

	UPROPERTY(VisibleAnywhere, Category="DerivedTextureData")
	FGuid Id;

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FDerivedTextureData& Data)
	{
		Ar << Data.TextureParameters;
		Ar << Data.Id;
		return Ar;
	}
};

// Tracks the state of the UTexture and it's source data
enum class EDerivedTextureState
{
	Invalid, // Has just been created and does not contain valid data
	Configured, // Has had texture settings and source ID set, but source data is not initialized
	SourceInitialized, // Source data has been initialized with dummy data
	SourceGenerated, // Source data has been filled with valid data
};

USTRUCT()
struct FDerivedTexture
{
	GENERATED_BODY()

	// UTexture where we store the actual image data
	UPROPERTY(VisibleAnywhere, Category="DerivedTexture")
	TObjectPtr<UTexture> Texture;

	// Metadata attached to the texture, which is cached to the DDC
	UPROPERTY(VisibleAnywhere, Category="DerivedTexture")
	FDerivedTextureData Data;

#if WITH_EDITOR
	EDerivedTextureState TextureState;

	// Use this critical section when editing any properties of the FDerivedTexture or UTexture
	TSharedPtr<FCriticalSection> TextureCS;
#endif

	FDerivedTexture()
		: Texture(nullptr)
		, Data()
#if WITH_EDITOR
		, TextureState(EDerivedTextureState::Invalid)
		, TextureCS(MakeShared<FCriticalSection>())
#endif
	{}
};

USTRUCT()
struct FDerivedParameterData
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category="DerivedParameter")
	FVector4f Value;

	// Hash of the data and logic used to produce this value
	UPROPERTY(VisibleAnywhere, Category="DerivedParameter")
	FGuid Id;

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FDerivedParameterData& Data)
	{
		Ar << Data.Value;
		Ar << Data.Id;
		return Ar;
	}
};

UCLASS()
class TEXTURESETSCOMMON_API UTextureSetDerivedData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category="DerivedData")
	TArray<FDerivedTexture> Textures;

	UPROPERTY(VisibleAnywhere, Category="DerivedData")
	TMap<FName, FDerivedParameterData> MaterialParameters;

#if WITH_EDITOR
	// Use this critical section when editing the MaterialParameters map
	FCriticalSection ParameterCS;
#endif

	// UObject Interface
	virtual void PostLoad() override;
};
