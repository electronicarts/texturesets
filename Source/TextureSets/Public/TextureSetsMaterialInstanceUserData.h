// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "TextureSetsMaterialInstanceUserData.generated.h"

class UTextureSet;
class UTextureSetDefinition;

#if WITH_EDITOR
class IDetailCategoryBuilder;
class UMaterialInstanceConstant;
#endif

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
	static void RegisterCallbacks();
	static void UnregisterCallbacks();

	// Ensures user data is in sync with the material
	static void UpdateAssetUserData(UMaterialInstance* MaterialInstance);

	// UObject Overrides
	virtual void PostInitProperties() override;
	virtual void PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext) override;

	// UAssetUserData Overrides
	virtual void PostLoadOwner() override;

	// Refreshes all texture set related material parameters based on the assigned material instances.
	void UpdateTextureSetParameters();

	// Removes all texture set related material parameters from the material instance
	void ClearTextureSetParameters();

	const FSetOverride& GetOverride(FGuid Guid) const;
	void SetOverride(FGuid Guid, const FSetOverride& Override);

private:
#if WITH_EDITOR
	// Callbacks
	static void OnMaterialInstanceOpenedForEdit(UMaterialInstance* MaterialInstance);
	static void OnMICreateGroupsWidget(TObjectPtr<UMaterialInstanceConstant> MaterialInstance, IDetailCategoryBuilder& GroupsCategory);

	static FDelegateHandle OnMaterialInstanceOpenedForEditHandle;
	static FDelegateHandle OnMICreateGroupsWidgetHandle;
#endif

	UPROPERTY(VisibleAnywhere)
	TMap<FGuid, FSetOverride> TexturesSetOverrides;

	UMaterialInstance* MaterialInstance;
	
};
