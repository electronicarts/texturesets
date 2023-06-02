// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "TextureSetsMaterialInstanceUserData.generated.h"

class UTextureSet;
class UTextureSetDefinition;

USTRUCT(BlueprintType)
struct FSetOverride
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	FName Name;

	UPROPERTY(VisibleAnywhere)
	UTextureSet* TextureSet;

	UPROPERTY(VisibleAnywhere)
	bool IsOverridden;
};

UCLASS(BlueprintType)
class TEXTURESETS_API UTextureSetsMaterialInstanceUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	// UObject Overrides
	virtual void PostInitProperties() override;
	virtual void PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext) override;

	// UAssetUserData Overrides
	virtual void PostLoadOwner() override;

	void UpdateTextureSetParameters();
	void ClearTextureSetParameters();

	UPROPERTY(VisibleAnywhere)
	TMap<FGuid, FSetOverride> TexturesSetOverrides;

private:
	UMaterialInstance* MaterialInstance;
	
};
