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

	void RegisterCustomizations();
	void UnregisterCustomizations();

	FDelegateHandle OnMaterialInstanceOpenedForEditHandle;

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;

	TSharedPtr<FTextureSetParameterEditor> ParameterEditor;

#if WITH_EDITOR
	FDelegateHandle OnGetExtraObjectTagsDelegateHandle;
	static void OnGetExtraObjectTags(const UObject* Object, TArray<UObject::FAssetRegistryTag>& OutTags);
#endif
};
