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
struct FTextureSetOverride
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FMaterialParameterInfo Info;

	UPROPERTY(VisibleAnywhere)
	UTextureSet* TextureSet;

	UPROPERTY(VisibleAnywhere)
	bool IsOverridden;
};

/// <summary>
/// Stores texture set override data on a material instance, since material instances can't directly reference texture sets.
/// On post-load, this applies all the individual material parameters to the material.
/// </summary>
UCLASS(BlueprintType)
class TEXTURESETS_API UTextureSetsMaterialInstanceUserData : public UAssetUserData
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	// Ensures user data is in sync with the material
	static void UpdateAssetUserData(UMaterialInstance* MaterialInstance);
	static const TMap<FMaterialParameterInfo, const UMaterialExpressionTextureSetSampleParameter*> FindAllSampleExpressions(UMaterialInstance* MaterialInstance);
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

	const TArray<FMaterialParameterInfo> GetOverrides() const;
	bool HasOverride(const FMaterialParameterInfo& Info) const;
	bool GetOverride(const FMaterialParameterInfo& Info, FTextureSetOverride& OutOverride) const;
	void SetOverride(const FTextureSetOverride& Override);
	void RemoveOverride(const FMaterialParameterInfo& Info);

private:
	UPROPERTY(VisibleAnywhere)
	TArray<FTextureSetOverride> TexturesSetOverrides;

	UMaterialInstance* MaterialInstance;
	
};
