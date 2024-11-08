// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/AssetRegistryTagsContext.h"

class IAssetTools;
class IAssetTypeActions;
class IDetailCategoryBuilder;
class UFactory;
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

	FDelegateHandle OnGetExtraObjectTagsDelegateHandle;
	static void OnGetExtraObjectTagsWithContext(FAssetRegistryTagsContext Context);
	
	FDelegateHandle OnPostEngineInitDelegateHandle;

	FDelegateHandle OnAssetPostImportDelegateHandle;
	static void OnAssetPostImport(UFactory* ImportFactory, UObject* InObject);

	void FixupTextureSetData();
};
