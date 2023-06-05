//
// (c) Electronic Arts.  All Rights Reserved.
//

#include "AssetTypeActions_TextureSetDefinition.h"

#include "AssetTypeActions_TextureSet.h"
#include "ContentBrowserModule.h"
#include "EditorSupportDelegates.h"
#include "FileHelpers.h"
#include "IContentBrowserSingleton.h"
#include "MaterialEditingLibrary.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetsAssetTypeActionsCommon.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

DEFINE_LOG_CATEGORY_STATIC(LogTextureSetDefFixMaterial, Log, Log);

namespace
{
	void FindAllTextureSets(UTextureSetDefinition* Definition, TArray<TObjectPtr<UTextureSet>>& OutTextureSets)
	{
		TArray<TObjectPtr<UTextureSet>> TextureSets;
		TextureSetsAssetTypeActionsCommon::GetReferencersOfType(Definition, TextureSets);
	
		for (TObjectPtr<UTextureSet> TextureSet : TextureSets)
		{
			OutTextureSets.AddUnique(TextureSet);
		}
	}
}

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

void FAssetTypeActions_TextureSetDefinition::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	if (InObjects.Num() == 1)
	{
		TArray<TWeakObjectPtr<UTextureSetDefinition>> Definitions = GetTypedWeakObjectPtrs<UTextureSetDefinition>(InObjects);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("TextureSetDefinition_FindTextureSets", "Find Texture Sets/Materials Using This"),
			LOCTEXT("TextureSetDefinition_FindTextureSetsTooltip", "Finds all materials and texture sets that use this definition in the content browser."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_TextureSetDefinition::ExecuteFindUsages, Definitions[0]),
				FCanExecuteAction()
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("TextureSet_FixMaterialUsage", "Fix Usage"),
			LOCTEXT("TextureSet_FixMaterialUsageTooltip", "Find texture sets and materials using this texture set and fixup."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_TextureSetDefinition::ExecuteFixUsages, Definitions[0]),
				FCanExecuteAction()
			)
		);
	}
}


void FAssetTypeActions_TextureSetDefinition::ExecuteFindUsages(TWeakObjectPtr<UTextureSetDefinition> Object)
{
	TArray<FAssetData> Usages;

	UTextureSetDefinition* Definition = Object.Get();
	if (Definition != nullptr)
	{
		TextureSetsAssetTypeActionsCommon::GetReferencersData(Definition, UMaterialInterface::StaticClass(), Usages);
		TextureSetsAssetTypeActionsCommon::GetReferencersData(Definition, UMaterialFunction::StaticClass(), Usages);
		TextureSetsAssetTypeActionsCommon::GetReferencersData(Definition, UTextureSet::StaticClass(), Usages);
	}

	if (Usages.Num() > 0)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(Usages);
	}
}

void FAssetTypeActions_TextureSetDefinition::ExecuteFixUsages(TWeakObjectPtr<UTextureSetDefinition> Object)
{

	TObjectPtr<UTextureSetDefinition> Definition = Object.Get();
	if (Definition != nullptr)
	{
		//FixUsages(Definition);

		FEditorDelegates::RefreshEditor.Broadcast();
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE
