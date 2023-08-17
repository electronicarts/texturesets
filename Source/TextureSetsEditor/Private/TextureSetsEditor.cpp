// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetsEditor.h"

#include "IMaterialEditor.h"
#include "IDetailGroup.h"
#include "DetailWidgetRow.h"
#include "AssetTypeActions/AssetTypeActions_TextureSet.h"
#include "AssetTypeActions/AssetTypeActions_TextureSetDefinition.h"
#include "MaterialPropertyHelpers.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialFunctionInterface.h"
#include "MaterialEditor/DEditorParameterValue.h"
#include "MaterialEditor/Public/MaterialEditorModule.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "IDetailTreeNode.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "FTextureSetsModule"

static TAutoConsoleVariable<bool> CVarShowTextureSetParameters(
	TEXT("ts.ShowTextureSetParameters"),
	false,
	TEXT("Makes texture set parameters visible in the material instance editor. These are usually hidden."));


class FTextureSetParameterEditor : public ICustomMaterialParameterEditor, public IMaterialParameterFilter
{
	virtual bool IsMaterialParameterHidden(TObjectPtr<UDEditorParameterValue> Param) override
	{
		if (CVarShowTextureSetParameters.GetValueOnAnyThread())
			return false;

		return UMaterialExpressionTextureSetSampleParameter::IsTextureSetParameterName(Param->ParameterInfo.Name);
	}

	virtual bool CanHandleParam(TObjectPtr<UDEditorCustomParameterValue> Param)
	{
		return Param.GetClass() == UDEditorTextureSetParameterValue::StaticClass();
	}

	virtual void CreateWidget(CreateWidgetArguments& Args)
	{
		UMaterialExpressionTextureSetSampleParameter* SamplerExpression = nullptr;

		if (Args.Parameter->ExpressionId.IsValid())
		{

			if (Args.Parameter->ParameterInfo.Association == EMaterialParameterAssociation::GlobalParameter)
			{
				if ( Args.Material != nullptr)
					SamplerExpression = Args.Material->FindExpressionByGUID<UMaterialExpressionTextureSetSampleParameter>(Args.Parameter->ExpressionId);
			}
			else if (Args.MaterialInstance)
			{
				FMaterialLayersFunctions Layers;
				Args.MaterialInstance->GetMaterialLayers(Layers);

				UMaterialFunctionInterface* LayerFunction = nullptr;

				if (Args.Parameter->ParameterInfo.Association == EMaterialParameterAssociation::LayerParameter)
				{
					LayerFunction = Layers.Layers[Args.Parameter->ParameterInfo.Index];
				}
				else if (Args.Parameter->ParameterInfo.Association == EMaterialParameterAssociation::BlendParameter)
				{
					LayerFunction = Layers.Blends[Args.Parameter->ParameterInfo.Index];
				}

				if (LayerFunction)
				{
					SamplerExpression = Args.Material->FindExpressionByGUID<UMaterialExpressionTextureSetSampleParameter>(Args.Parameter->ExpressionId, LayerFunction);
				}
			}
		}

		if (IsValid(SamplerExpression))
		{
			TSharedPtr<SVerticalBox> NameVerticalBox;
			const FText ParameterName = !Args.NameOverride.IsEmpty() ? Args.NameOverride : FText::FromName(Args.Parameter->ParameterInfo.Name);
		
			FDetailWidgetRow& CustomWidget = Args.Row->CustomWidget();
			CustomWidget
				.FilterString(ParameterName)
				.NameContent()
				[
					SAssignNew(NameVerticalBox, SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
				[
					SNew(STextBlock)
					.Text(ParameterName)
					.ToolTipText(FText::FromString(Args.Parameter->Description))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				];
			CustomWidget.ValueContent()
				[
					SNew(SObjectPropertyEntryBox)
					.PropertyHandle(Args.ParameterValueProperty)
					.AllowedClass(UTextureSet::StaticClass())
					.DisplayThumbnail(true)
					.ThumbnailPool(UThumbnailManager::Get().GetSharedThumbnailPool())
					.OnShouldFilterAsset_Lambda([SamplerExpression](const FAssetData& AssetData)
						{
							if (!SamplerExpression)
								return false;
		
							UTextureSet* TextureSetAsset = CastChecked<UTextureSet>(AssetData.GetAsset());
							return TextureSetAsset->Definition != SamplerExpression->Definition;
		
							// TODO: Why isn't this working?
							//const FString& AssetDefinitionID = AssetData.GetTagValueRef<FString>("TextureSetDefinitionID");
							//const FString ExpressionDefinitionID = SamplerExpression->Definition->GetGuid().ToString();
							//return AssetDefinitionID != ExpressionDefinitionID;
						})
				];
		}
	}
};

void FTextureSetsEditorModule::StartupModule()
{
	RegisterAssetTools();

	ParameterEditor = MakeShared<FTextureSetParameterEditor>();
	FMaterialPropertyHelpers::RegisterParameterFilter(ParameterEditor.Get());
	FMaterialPropertyHelpers::RegisterCustomParameterEditor(ParameterEditor.Get());
}

void FTextureSetsEditorModule::ShutdownModule()
{
	UnregisterAssetTools();

	FMaterialPropertyHelpers::UnregisterParameterFilter(ParameterEditor.Get());
	FMaterialPropertyHelpers::UnregisterCustomParameterEditor(ParameterEditor.Get());
	ParameterEditor.Reset();
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

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTextureSetsEditorModule, TextureSets)
