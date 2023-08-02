// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class IAssetTools;
class IAssetTypeActions;
class IDetailCategoryBuilder;
class UMaterialInstanceConstant;
class IMaterialParameterFilter;

class FTextureSetsEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	void RegisterAssetTools();
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);
	void UnregisterAssetTools();

	void RegisterCallbacks();
	void UnregisterCallbacks();

	// Callbacks
	void OnMaterialInstanceOpenedForEdit(UMaterialInstance* MaterialInstance);
	void OnMICreateGroupsWidget(TObjectPtr<UMaterialInstanceConstant> MaterialInstance, IDetailCategoryBuilder& GroupsCategory);

	FDelegateHandle OnMaterialInstanceOpenedForEditHandle;
	FDelegateHandle OnMICreateGroupsWidgetHandle;

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;

	TSharedPtr<IMaterialParameterFilter> ParameterFilter;
};
