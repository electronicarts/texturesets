// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class IAssetTools;
class IAssetTypeActions;
class IDetailCategoryBuilder;
class UMaterialInstanceConstant;
class FTextureSetParameterEditor;

class FTextureSetsEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	void RegisterAssetTools();
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);
	void UnregisterAssetTools();

	FDelegateHandle OnMaterialInstanceOpenedForEditHandle;

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;

	TSharedPtr<FTextureSetParameterEditor> ParameterEditor;
};
