// (c) Electronic Arts.  All Rights Reserved.

#include "AssetTypeActions_TextureSet.h"

#include "TextureSet.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_TextureSet::FAssetTypeActions_TextureSet()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetCategoryBit = AssetTools.FindAdvancedAssetCategory(FName(TEXT("TextureSets")));
}

UClass* FAssetTypeActions_TextureSet::GetSupportedClass() const
{ 
	return UTextureSet::StaticClass();
}

FText FAssetTypeActions_TextureSet::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_TextureSet", "Texture Set");
}

FColor FAssetTypeActions_TextureSet::GetTypeColor() const
{ 
	return FColor(192, 64, 64);
}

bool FAssetTypeActions_TextureSet::CanFilter()
{ 
	return true;
}

#undef LOCTEXT_NAMESPACE
