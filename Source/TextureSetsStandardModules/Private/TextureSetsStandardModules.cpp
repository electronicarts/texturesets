// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetsStandardModules.h"

#include "Interfaces/IPluginManager.h"
#include "ElementCollection/AssetTypeActions_TextureSetElementCollection.h"
#include "IAssetTools.h"

#define LOCTEXT_NAMESPACE "FTextureSetsModule"

void FTextureSetsStandardModulesModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	RegisteredAssetTypeActions.Add(Action);
}

void FTextureSetsStandardModulesModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_TextureSetElementCollection));
}

void FTextureSetsStandardModulesModule::ShutdownModule()
{
	if (FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools"))
	{
		IAssetTools& AssetTools = AssetToolsModule->Get();

		for (const TSharedRef<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action);
		}

		RegisteredAssetTypeActions.Empty();
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTextureSetsStandardModulesModule, TextureSetsStandardModules)
