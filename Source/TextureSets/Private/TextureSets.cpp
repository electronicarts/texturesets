//
// (c) Electronic Arts.  All Rights Reserved.
//

#include "TextureSets.h"

#include "IMaterialEditor.h"
#include "MaterialEditorUtilities.h"
#include "MaterialPropertyHelpers.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialGraphNode_Invisible.h"
#include "MaterialGraphNode_TSSampler.h"
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

	UMaterialGraph::PreAddExpression.BindRaw(this, &FTextureSetsModule::OnAddExpression);
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

void FTextureSetsModule::OnMIPostEditProperty(UMaterialInstance* MaterialInstancePtr, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::ValueSet || !PropertyChangedEvent.GetPropertyName().IsEqual("AssetUserData"))
	{
		return;
	}

	FTextureSetEditingUtils::UpdateMaterialInstance(MaterialInstancePtr);
}

void FTextureSetsModule::OnMaterialInstanceOpenedForEdit(UMaterialInstance* MaterialInstancePtr)
{
	UMaterialInstance* MaterialInstance = MaterialInstancePtr;
	if (!MaterialInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("No material instance found"));
		return;
	}
	
	for (auto Expression : MaterialInstance->GetMaterial()->GetExpressions())
	{
		if (Expression->IsA(UMaterialExpressionTextureSetSampleParameter::StaticClass()))
		{
			UTextureSetAssetUserData * TextureSetOverrides = MaterialInstancePtr->GetAssetUserData<UTextureSetAssetUserData>();
			if (!TextureSetOverrides)
			{
				TextureSetOverrides = NewObject<UTextureSetAssetUserData>();
				MaterialInstance->AddAssetUserData(TextureSetOverrides);
			}

			FGuid Guid = Cast<UMaterialExpressionTextureSetSampleParameter>(Expression)->MaterialExpressionGuid;
			auto OverrideInstance = TextureSetOverrides->TexturesSetOverrides.FindByPredicate([Guid](const FSetOverride& SetOverride) { return SetOverride.Guid == Guid; });
			if (OverrideInstance)
			{
				OverrideInstance->DefaultTextureSet = Cast<UMaterialExpressionTextureSetSampleParameter>(Expression)->TextureSet;
				OverrideInstance->Name = Cast<UMaterialExpressionTextureSetSampleParameter>(Expression)->Name;
				continue;
			}

			FSetOverride override;
			override.Name = Cast<UMaterialExpressionTextureSetSampleParameter>(Expression)->Name;
			override.TextureSet = Cast<UMaterialExpressionTextureSetSampleParameter>(Expression)->TextureSet;
			override.DefaultTextureSet = Cast<UMaterialExpressionTextureSetSampleParameter>(Expression)->TextureSet;
			override.Guid = Guid;
			override.IsOverridden = false;
			
			TextureSetOverrides->AddOverride(override);
		}
	}

	MaterialInstance->OnPostEditChangeProperty().AddRaw(this, &FTextureSetsModule::OnMIPostEditProperty);
	
}

UMaterialGraphNode* FTextureSetsModule::OnAddExpression(UMaterialExpression* Expression, UMaterialGraph* Graph)
{
	// Node for UMaterialExpressionExecBegin is explicitly placed if needed
	// We don't created any node for UMaterialExpressionExecEnd, it's handled as part of the root node
	UMaterialGraphNode* Node = nullptr;
	
	if (Expression && !Node &&
		!Expression->IsA(UMaterialExpressionExecBegin::StaticClass()) &&
		!Expression->IsA(UMaterialExpressionExecEnd::StaticClass()))
	{
		Graph->Modify();

		if (Expression->IsA(UMaterialExpressionTextureReferenceInternal::StaticClass()))
		{
			Node = FTextureSetEditingUtils::InitExpressionNewNode<UMaterialGraphNode_Invisible>(Graph, Expression, false);
		}
		else if (Expression->IsA(UMaterialExpressionTextureSetSampleParameter::StaticClass()))
		{
			Node = FTextureSetEditingUtils::InitExpressionNewNode<UMaterialGraphNode_TSSampler>(Graph, Expression, false);
		}
		
	}
	
	return Node;
}

void FTextureSetsModule::OnMICreateGroupsWidget(TObjectPtr<UMaterialInstanceConstant> MaterialInstancePtr, IDetailCategoryBuilder& GroupsCategory)
{
	UMaterialInstanceConstant* MaterialInstance = MaterialInstancePtr;
	if (!MaterialInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("No material instance found"));
		return;
	}
	UTextureSetAssetUserData* TextureSetOverrides = MaterialInstance->GetAssetUserData<UTextureSetAssetUserData>();
	if (!TextureSetOverrides)
	{
		UE_LOG(LogTemp, Warning, TEXT("No TextureSetAsset UserData found"));
		return;
	}

	const FName& GroupName = FMaterialPropertyHelpers::TextureSetParamName;
	IDetailGroup& DetailGroup = GroupsCategory.AddGroup(GroupName, FText::FromName(GroupName), false, true);

	DetailGroup.HeaderRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromName(DetailGroup.GetGroupName()))
		];

	for (UINT ParameterIndex = 0; ParameterIndex < (UINT)TextureSetOverrides->TexturesSetOverrides.Num(); ParameterIndex++)
	{
		DetailGroup.AddWidgetRow()
			[
				SNew(STextureSetParameterWidget, (UMaterialInstance*)MaterialInstance, ParameterIndex)
			];
	}

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTextureSetsModule, TextureSets)
