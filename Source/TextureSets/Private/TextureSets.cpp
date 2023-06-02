// (c) Electronic Arts. All Rights Reserved.

#include "TextureSets.h"

#include "IMaterialEditor.h"
#include "MaterialEditorUtilities.h"
#include "MaterialPropertyHelpers.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "TextureSet.h"
#include "TextureSetAssetUserData.h"
#include "TextureSetEditingUtils.h"
#include "MaterialEditor/Public/MaterialEditorModule.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "AssetTypeActions/AssetTypeActions_TextureSet.h"
#include "AssetTypeActions/AssetTypeActions_TextureSetDefinition.h"
#include "MaterialGraph/MaterialGraph.h"
#include "Materials/MaterialExpressionExecBegin.h"
#include "Materials/MaterialExpressionExecEnd.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "STextureSetParameterWidget.h"

#define LOCTEXT_NAMESPACE "FTextureSetsModule"

TAutoConsoleVariable<bool> CVarVisualizeMaterialGraph(
	TEXT("TextureSet.VisualizeInMaterialGraph"),
	false,
	TEXT("Draw debug widgets in material graphs. Requires graphs to be re-opened\n"),
	ECVF_Default);

void FTextureSetsModule::StartupModule()
{
	RegisterAssetTools();
	// This is how we could extend a toolbar
	IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
	MaterialEditorModule->OnMaterialInstanceOpenedForEdit().AddRaw(this, &FTextureSetsModule::OnMaterialInstanceOpenedForEdit);

	UMaterialEditorInstanceConstant::OnCreateGroupsWidget.BindRaw(this, &FTextureSetsModule::OnMICreateGroupsWidget);
}

void FTextureSetsModule::ShutdownModule()
{
	UnregisterAssetTools();
}

void FTextureSetsModule::RegisterAssetTools()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("TextureSets")), LOCTEXT("TextureSetsAssetCategory", "Texture Sets") );

	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_TextureSet));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_TextureSetDefinition));
}

void FTextureSetsModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	RegisteredAssetTypeActions.Add(Action);
}

void FTextureSetsModule::UnregisterAssetTools()
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

void FTextureSetsModule::UpdateAssetUserData(UMaterialInstance* MaterialInstance)
{
	check(MaterialInstance);

	if (!IsValid(MaterialInstance->GetMaterial()))
		return; // Possible if parent has not been assigned yet.

	TArray<const UMaterialExpressionTextureSetSampleParameter*> TextureSetExpressions;
	MaterialInstance->GetMaterial()->GetAllExpressionsOfType<UMaterialExpressionTextureSetSampleParameter>(TextureSetExpressions);

	UTextureSetAssetUserData* TextureSetUserData = MaterialInstance->GetAssetUserData<UTextureSetAssetUserData>();

	// If there are no texture set expressions, we can clear TextureSetUserData
	if (TextureSetExpressions.IsEmpty() && IsValid(TextureSetUserData))
	{
		MaterialInstance->RemoveUserDataOfClass(UTextureSetAssetUserData::StaticClass());
		return; // Done here, not a material that needs texture set stuff.
	}

	TArray<FGuid> UnusedOverrides;

	if (IsValid(TextureSetUserData))
		TextureSetUserData->TexturesSetOverrides.GetKeys(UnusedOverrides);

	for (const UMaterialExpressionTextureSetSampleParameter* TextureSetExpression : TextureSetExpressions)
	{
		check(TextureSetExpression);

		// Add texture set user data if it doesn't exist
		// Note that this happens inside the loop of texture set expressions.
		// This is so the user data is only added if there is a texture set expression.
		if (!IsValid(TextureSetUserData))
		{
			TextureSetUserData = NewObject<UTextureSetAssetUserData>(MaterialInstance);
			MaterialInstance->AddAssetUserData(TextureSetUserData);
		}

		const FGuid NodeGuid = TextureSetExpression->MaterialExpressionGuid;
		auto OverrideInstance = TextureSetUserData->TexturesSetOverrides.Find(NodeGuid);
		if (OverrideInstance)
		{
			// Update existing override with the correct value
			OverrideInstance->Name = TextureSetExpression->ParameterName;
			UnusedOverrides.RemoveSingle(NodeGuid);
		}
		else
		{
			// Add new override
			FSetOverride Override;

			// TODO: See if there are any old overrides with the same name, which we could use to recover values from.
			Override.Name = TextureSetExpression->ParameterName;
			Override.TextureSet = TextureSetExpression->DefaultTextureSet;
			Override.IsOverridden = false;

			TextureSetUserData->TexturesSetOverrides.Add(NodeGuid, Override);
		}
	}

	// Clear any overrides we no longer need.
	for (FGuid Unused : UnusedOverrides)
		TextureSetUserData->TexturesSetOverrides.Remove(Unused);
}

void FTextureSetsModule::OnMaterialInstanceOpenedForEdit(UMaterialInstance* MaterialInstance)
{
	UpdateAssetUserData(MaterialInstance);
}

void FTextureSetsModule::OnMICreateGroupsWidget(TObjectPtr<UMaterialInstanceConstant> MaterialInstance, IDetailCategoryBuilder& GroupsCategory)
{
	check(MaterialInstance);

	UTextureSetAssetUserData* TextureSetOverrides = MaterialInstance->GetAssetUserData<UTextureSetAssetUserData>();
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

	for (const auto& [Guid, Override] : TextureSetOverrides->TexturesSetOverrides)
	{
		DetailGroup.AddWidgetRow()
			[
				SNew(STextureSetParameterWidget, MaterialInstance, Guid)
			];
	}

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTextureSetsModule, TextureSets)
