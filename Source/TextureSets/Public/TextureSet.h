//
// (c) Electronic Arts.  All Rights Reserved.
//

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "TextureSet.generated.h"


class UTexture;
class UTextureSetDefinition;

TObjectPtr<UTexture2D> LoadDefaultTexture();

USTRUCT(BlueprintType)
struct FTextureData
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite)
	FString TextureName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	TObjectPtr<class UTexture> TextureAsset = LoadDefaultTexture();
	UPROPERTY()
	TArray< TObjectPtr<UAssetUserData> > AssetUserData;
};

UCLASS(BlueprintType, hidecategories = (Object))
class TEXTURESETS_API UTextureSet : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Compositing)
	TObjectPtr<UTextureSetDefinition> Definition;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, EditFixedSize, Category = Compositing)
	TArray<FTextureData> Textures;
	
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent & PropertyChangedEvent) override;

	void UpdateFromDefinition();
	void UpdateReferencingMaterials();

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TMap<FName, FTextureSource> SourceTextures;

	UPROPERTY()
	TMap<FName, FVector4> SourceParameters;
#endif

	UPROPERTY()
	TArray<UTexture*> CookedTextures;
	
	UPROPERTY()
	TMap<FName, FVector4> ShaderParameters;
};
