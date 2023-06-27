// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TextureSetDerivedData.generated.h"

USTRUCT()
struct TEXTURESETS_API FPackedTextureData
{
	GENERATED_BODY()

public:
	// Material parameters that were generated along with this packed texture
	UPROPERTY(VisibleAnywhere)
	TMap<FName, FVector4> MaterialParameters;

	// Hashed value computed when this texture was built
	UPROPERTY(VisibleAnywhere)
	FString Key;

	// Guid generated from the key
	UPROPERTY(VisibleAnywhere)
	FGuid Id;
};

UCLASS(Within=TextureSet)
class TEXTURESETS_API UTextureSetDerivedData : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere)
	TMap<FName, FVector4> MaterialParameters;

	UPROPERTY(VisibleAnywhere)
	FString Key;

	UPROPERTY(VisibleAnywhere)
	TArray<FPackedTextureData> PackedTextureData;

};
