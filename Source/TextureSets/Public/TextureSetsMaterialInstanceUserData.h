// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "TextureSetsMaterialInstanceUserData.generated.h"

class UTextureSet;
class UTextureSetDefinition;
class UMaterialExpressionTextureSetSampleParameter;
class UMaterial;
class UMaterialInstance;

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

#if WITH_EDITOR
	// Ensures user data is in sync with the material
	static void UpdateAssetUserData(UMaterialInstance* MaterialInstance);
	static const UMaterialExpressionTextureSetSampleParameter* FindSampleExpression(const FGuid& NodeID, UMaterial* Material);
#endif

	// UObject Overrides
	virtual void PostInitProperties() override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

	// UAssetUserData Overrides
	virtual void PostLoadOwner() override;

#if WITH_EDITOR
	// Refreshes all texture set related material parameters based on the assigned material instances.
	void UpdateTextureSetParameters();
#endif

	// Removes all texture set related material parameters from the material instance
	void ClearTextureSetParameters();

	const TArray<FGuid> GetOverrides() const;
	const FSetOverride& GetOverride(FGuid Guid) const;
	void SetOverride(FGuid Guid, const FSetOverride& Override);

private:
	UPROPERTY(VisibleAnywhere)
	TMap<FGuid, FSetOverride> TexturesSetOverrides;

	UMaterialInstance* MaterialInstance;
	
};
