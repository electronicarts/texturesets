// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TextureSetDerivedData.generated.h"

USTRUCT()
struct TEXTURESETS_API FDerivedTextureData
{
	GENERATED_BODY()

public:
	// Material parameters that were generated along with this packed texture
	UPROPERTY(VisibleAnywhere)
	TMap<FName, FVector4f> MaterialParameters;

	// Guid generated from the key
	UPROPERTY(VisibleAnywhere)
	FGuid Id;
};

USTRUCT()
struct TEXTURESETS_API FDerivedParameterData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	FName Name;

	UPROPERTY(VisibleAnywhere)
	FVector4f Value;

	// Hash of the data and logic used to produce this value
	UPROPERTY(VisibleAnywhere)
	uint32 Hash;
};

UCLASS(Within=TextureSet, DefaultToInstanced)
class TEXTURESETS_API UTextureSetDerivedData : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, Category="Derived Data")
	TArray<FDerivedParameterData> MaterialParameters;

	UPROPERTY(VisibleAnywhere, Category="Derived Data")
	TArray<FDerivedTextureData> DerivedTextureData;

	UPROPERTY(VisibleAnywhere, Category="Derived Data")
	FGuid Id;
};
