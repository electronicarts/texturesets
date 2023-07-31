// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetsEditor.h"

#include "IMaterialEditor.h"
#include "IDetailGroup.h"
#include "DetailWidgetRow.h"
#include "STextureSetParameterWidget.h"
#include "AssetTypeActions/AssetTypeActions_TextureSet.h"
#include "AssetTypeActions/AssetTypeActions_TextureSetDefinition.h"
#include "MaterialPropertyHelpers.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialEditor/Public/MaterialEditorModule.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "TextureSetsMaterialInstanceUserData.h"

#define LOCTEXT_NAMESPACE "FTextureSetsModule"

void FTextureSetsEditorModule::StartupModule()
{
	RegisterAssetTools();
	RegisterCallbacks();
}

void FTextureSetsEditorModule::ShutdownModule()
{
	UnregisterCallbacks();
	UnregisterAssetTools();
}

void FTextureSetsEditorModule::RegisterAssetTools()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("TextureSets")), LOCTEXT("TextureSetsAssetCategory", "Texture Sets") );

	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_TextureSet));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_TextureSetDefinition));
}

void FTextureSetsEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	RegisteredAssetTypeActions.Add(Action);
}

void FTextureSetsEditorModule::UnregisterAssetTools()
{
	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");

	if (AssetToolsModule != nullptr)
	{
		IAssetTools& AssetTools = AssetToolsModule->Get();

		for (auto Action : RegisteredAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action);
		}
	}
}

void FTextureSetsEditorModule::RegisterCallbacks()
{
	check(!OnMaterialInstanceOpenedForEditHandle.IsValid());
	check(!OnMICreateGroupsWidgetHandle.IsValid());

	IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>("MaterialEditor");
	OnMaterialInstanceOpenedForEditHandle = MaterialEditorModule->OnMaterialInstanceOpenedForEdit().AddRaw(this, &FTextureSetsEditorModule::OnMaterialInstanceOpenedForEdit);

	OnMICreateGroupsWidgetHandle = UMaterialEditorInstanceConstant::OnCreateGroupsWidget.AddRaw(this, &FTextureSetsEditorModule::OnMICreateGroupsWidget);
}

void FTextureSetsEditorModule::UnregisterCallbacks()
{
	check(OnMaterialInstanceOpenedForEditHandle.IsValid());
	check(OnMICreateGroupsWidgetHandle.IsValid());

	if (OnMaterialInstanceOpenedForEditHandle.IsValid())
	{
		IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>("MaterialEditor");
		MaterialEditorModule->OnMaterialInstanceOpenedForEdit().Remove(OnMaterialInstanceOpenedForEditHandle);
		OnMaterialInstanceOpenedForEditHandle.Reset();
	}

	if (OnMICreateGroupsWidgetHandle.IsValid())
	{
		UMaterialEditorInstanceConstant::OnCreateGroupsWidget.Remove(OnMICreateGroupsWidgetHandle);
		OnMICreateGroupsWidgetHandle.Reset();
	}
}

void FTextureSetsEditorModule::OnMaterialInstanceOpenedForEdit(UMaterialInstance* MaterialInstance)
{
	UTextureSetsMaterialInstanceUserData::UpdateAssetUserData(MaterialInstance);
}

void FTextureSetsEditorModule::OnMICreateGroupsWidget(TObjectPtr<UMaterialInstanceConstant> MaterialInstance, IDetailCategoryBuilder& GroupsCategory)
{
	check(MaterialInstance);

	UTextureSetsMaterialInstanceUserData* TextureSetOverrides = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();
	// Not an error if user data doesn't exist, just means this material doesn't contain a texture set.
	if (!TextureSetOverrides)
		return;

	const FName& GroupName = FMaterialPropertyHelpers::TextureSetParamName;
	IDetailGroup& DetailGroup = GroupsCategory.AddGroup(GroupName, FText::FromName(GroupName), false, true);

	DetailGroup.HeaderRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromName(DetailGroup.GetGroupName()))
		];

	for (const FName& ParameterName : TextureSetOverrides->GetOverrides())
	{
		DetailGroup.AddWidgetRow()
			[
				SNew(STextureSetParameterWidget, MaterialInstance, ParameterName)
			];
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTextureSetsEditorModule, TextureSets)
