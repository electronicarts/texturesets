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
	TMap<FName, FVector4f> TextureParameters;

	// Guid generated from the key
	UPROPERTY(VisibleAnywhere)
	FGuid Id;

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FDerivedTextureData& Data)
	{
		Ar << Data.TextureParameters;
		Ar << Data.Id;
		return Ar;
	}
};

USTRUCT()
struct TEXTURESETS_API FDerivedParameterData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	FVector4f Value;

	// Hash of the data and logic used to produce this value
	UPROPERTY(VisibleAnywhere)
	FGuid Id;

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FDerivedParameterData& Data)
	{
		Ar << Data.Value;
		Ar << Data.Id;
		return Ar;
	}
};

USTRUCT()
struct TEXTURESETS_API FTextureSetDerivedData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, NoClear)
	TArray<TObjectPtr<UTexture>> Textures;

	UPROPERTY(VisibleAnywhere)
	TArray<FDerivedTextureData> TextureData;

	UPROPERTY(VisibleAnywhere)
	TMap<FName, FDerivedParameterData> MaterialParameters;

#if WITH_EDITOR
	bool bIsCompiling = false;
#endif

	inline bool IsValid() const
	{
#if WITH_EDITOR
		if (bIsCompiling)
			return false;
#endif

		if (Textures.Num() == 0 && MaterialParameters.Num() == 0)
			return false;

		if (Textures.Num() != TextureData.Num())
			return false;

		return true;
	}
};
