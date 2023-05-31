// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "TextureSetAssetUserData.generated.h"

class UTextureSet;
class UTextureSetDefinition;

USTRUCT(BlueprintType)
struct FSetOverride
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	FName Name;

	UPROPERTY(EditAnywhere)
	UTextureSet* TextureSet;

	UPROPERTY()
	FGuid MaterialExpressionGuid;

	UPROPERTY()
	bool IsOverridden;
};

UCLASS(BlueprintType)
class TEXTURESETS_API UTextureSetAssetUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	UPROPERTY(EditAnywhere)
	TArray<FSetOverride> TexturesSetOverrides;
	
};
