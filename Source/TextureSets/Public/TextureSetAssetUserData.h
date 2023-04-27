// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "TextureSetAssetUserData.generated.h"

class UTextureSet;

/**
 * 
 */

USTRUCT(BlueprintType)
struct FSetOverride
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	FName Name;
	UPROPERTY(EditAnywhere)
	UTextureSet* TextureSet;

	UPROPERTY()
	UTextureSet* DefaultTextureSet;
	UPROPERTY()
	FGuid Guid;
	UPROPERTY()
	bool IsOverridden;
	// UPROPERTY()
	// TArray< TObjectPtr<UAssetUserData> > AssetUserData;
};

UCLASS(BlueprintType)
class TEXTURESETS_API UTextureSetAssetUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSetOverride> TexturesSetOverrides;
	

	UTextureSetAssetUserData(){}
	void AddOverride(FSetOverride& Override);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	
};
