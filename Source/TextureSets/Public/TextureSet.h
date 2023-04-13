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

USTRUCT(BlueprintType)
struct FTextureData
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite)
	FString TextureName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UTexture> TextureAsset;
	UPROPERTY()
	TArray< TObjectPtr<UAssetUserData> > AssetUserData;
};

UCLASS(BlueprintType, hidecategories = (Object))
class TEXTURESETS_API UTextureSet : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	void test();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Compositing)
	TObjectPtr<UTextureSetDefinition> Definition;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, EditFixedSize, Category = Compositing)
	TArray<FTextureData> Textures;
	
	
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent & PropertyChangedEvent);

	void UpdateFromDefinition();
	void UpdateReferencingMaterials();

	/*--------------------------------------------------------------------------
	Editor only properties used to build the runtime texture data.
--------------------------------------------------------------------------*/

//#if WITH_EDITORONLY_DATA
//	UPROPERTY()
//		FTextureSource Source;
//#endif
};
