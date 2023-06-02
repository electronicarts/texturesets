// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Modules/ModuleManager.h"

class IMaterialEditor;
class IAssetTools;
class IAssetTypeActions;
class IDetailCategoryBuilder;
class UMaterialInstanceConstant;

class FTextureSetsModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	void RegisterAssetTools();
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);
	void UnregisterAssetTools();

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;

	// Ensures user data is in sync with the material
	void UpdateAssetUserData(UMaterialInstance* MaterialInstance);

	// Callbacks
	void OnMaterialInstanceOpenedForEdit(UMaterialInstance* MaterialInstance);
	void OnMICreateGroupsWidget(TObjectPtr<UMaterialInstanceConstant> MaterialInstance, IDetailCategoryBuilder& GroupsCategory);

};
