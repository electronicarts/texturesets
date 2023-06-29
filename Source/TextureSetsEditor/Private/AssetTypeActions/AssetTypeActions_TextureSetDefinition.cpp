// (c) Electronic Arts.  All Rights Reserved.

#include "AssetTypeActions_TextureSetDefinition.h"

#include "TextureSetDefinition.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_TextureSetDefinition::FAssetTypeActions_TextureSetDefinition()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetCategoryBit = AssetTools.FindAdvancedAssetCategory(FName(TEXT("TextureSets")));
}

UClass* FAssetTypeActions_TextureSetDefinition::GetSupportedClass() const
{
	return UTextureSetDefinition::StaticClass();
}

FText FAssetTypeActions_TextureSetDefinition::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_TextureSetDefinition", "Texture Set Definition");
}

FColor FAssetTypeActions_TextureSetDefinition::GetTypeColor() const
{
	return FColor(192, 64, 64);
}

bool FAssetTypeActions_TextureSetDefinition::CanFilter()
{
	return true;
}

#undef LOCTEXT_NAMESPACE
