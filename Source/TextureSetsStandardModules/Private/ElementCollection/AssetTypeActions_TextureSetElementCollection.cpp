// (c) Electronic Arts.  All Rights Reserved.

#include "AssetTypeActions_TextureSetElementCollection.h"

#include "ElementCollection/TextureSetElementCollection.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_TextureSetElementCollection::FAssetTypeActions_TextureSetElementCollection()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetCategoryBit = AssetTools.FindAdvancedAssetCategory(FName(TEXT("TextureSets")));
}

UClass* FAssetTypeActions_TextureSetElementCollection::GetSupportedClass() const
{ 
	return UTextureSetElementCollectionAsset::StaticClass();
}

FText FAssetTypeActions_TextureSetElementCollection::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_TextureSetElementCollection", "Texture Set Element Collection");
}

FColor FAssetTypeActions_TextureSetElementCollection::GetTypeColor() const
{ 
	return FColor(192, 64, 64);
}

bool FAssetTypeActions_TextureSetElementCollection::CanFilter()
{ 
	return true;
}

#undef LOCTEXT_NAMESPACE
