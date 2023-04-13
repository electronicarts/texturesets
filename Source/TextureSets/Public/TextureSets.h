//
// (c) Electronic Arts.  All Rights Reserved.
//

#pragma once

#include "CoreMinimal.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Modules/ModuleManager.h"

class IMaterialEditor;
class IAssetTools;
class IAssetTypeActions;

// -----

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

	// Callbacks
	void OnMaterialInstanceOpenedForEdit(UMaterialInstance*);
	UMaterialGraphNode* OnAddExpression(UMaterialExpression* Expression, UMaterialGraph* Graph);
	void OnMIPostEditProperty(UMaterialInstance* MaterialInstancePtr, FPropertyChangedEvent& PropertyChangedEvent);
};
