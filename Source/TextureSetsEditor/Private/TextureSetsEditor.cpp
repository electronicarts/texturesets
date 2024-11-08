// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetsEditor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "AssetTypeActions/AssetTypeActions_TextureSet.h"
#include "AssetTypeActions/AssetTypeActions_TextureSetDefinition.h"
#include "DEditorTextureSetParameterValue.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "IDetailTreeNode.h"
#include "IMaterialEditor.h"
#include "Interfaces/IPluginManager.h"
#include "MaterialEditor/DEditorParameterValue.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "MaterialEditorModule.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialPropertyHelpers.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PropertyCustomizationHelpers.h"
#include "TextureSet.h"
#include "TextureSetAssetParamsCollectionCustomization.h"
#include "TextureSetDefinition.h"
#include "TextureSetSourceTextureReferenceCustomization.h"
#include "TextureSetThumbnailRenderer.h"
#include "TextureSetsHelpers.h"
#include "UObject/Object.h"
#include "UObject/AssetRegistryTagsContext.h"

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

		return TextureSetsHelpers::IsTextureSetParameterName(Param->ParameterInfo.Name);
	}

	virtual bool CanHandleParam(TObjectPtr<UDEditorCustomParameterValue> Param) override
	{
		return Param.GetClass() == UDEditorTextureSetParameterValue::StaticClass();
	}

	virtual void CreateWidget(CreateWidgetArguments& Args) override
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
			const FText ParameterName = FText::Format(INVTEXT("{0}\n({1})"),
				!Args.NameOverride.IsEmpty() ? Args.NameOverride : FText::FromName(Args.Parameter->ParameterInfo.Name),
				FText::FromString(SamplerExpression->Definition.GetName()));

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

							FAssetTagValueRef DefinitionIdValue = AssetData.TagsAndValues.FindTag("TextureSetDefinitionID");
							if (DefinitionIdValue.IsSet())
							{
								// Prefer to compare with the asset tag to avoid loading each texture set just to check it's definition.
								const FString AssetDefinitionID = DefinitionIdValue.AsString();
								const FString ExpressionDefinitionID = SamplerExpression->Definition->GetGuid().ToString();
								return AssetDefinitionID != ExpressionDefinitionID;
							}
							else
							{
								// No definition ID was found, so we need to load the texture set to know if it references the right definition.
								UTextureSet* TextureSetAsset = CastChecked<UTextureSet>(AssetData.GetAsset());
								return TextureSetAsset->Definition != SamplerExpression->Definition;
							}
						})
				];
		}
	}
};

void FTextureSetsEditorModule::StartupModule()
{
	RegisterAssetTools();
	RegisterCustomizations();

	ParameterEditor = MakeShared<FTextureSetParameterEditor>();
	FMaterialPropertyHelpers::RegisterParameterFilter(ParameterEditor);
	FMaterialPropertyHelpers::RegisterCustomParameterEditor(ParameterEditor);

	OnGetExtraObjectTagsDelegateHandle = UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.AddStatic(&FTextureSetsEditorModule::OnGetExtraObjectTagsWithContext);

	// Register delegates during PostEngineInit as GEditor is not valid yet
	OnPostEngineInitDelegateHandle = FCoreDelegates::OnPostEngineInit.AddLambda([this]()
	{
		if (GEditor)
		{
			OnAssetPostImportDelegateHandle = GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.AddStatic(&FTextureSetsEditorModule::OnAssetPostImport);
		}
	});

	UThumbnailManager::Get().RegisterCustomRenderer(UTextureSet::StaticClass(), UTextureSetThumbnailRenderer::StaticClass());

	UToolMenu* ToolMenu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu.Tools");
	if (ToolMenu)
	{
		FToolMenuSection& Section = ToolMenu->AddSection("Texture Sets", FText::FromString(TEXT("Texture Sets")));
		Section.AddMenuEntry("FixupTextureSetData",
			LOCTEXT("FixupTextureSetData_Label", "Fixup Texture Set Data"),
			LOCTEXT("FixupTextureSetData_Desc", "Update all texture set data that needs patching"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FTextureSetsEditorModule::FixupTextureSetData), FCanExecuteAction())
		);
	}
}

void FTextureSetsEditorModule::ShutdownModule()
{
	UnregisterAssetTools();
	UnregisterCustomizations();

	FMaterialPropertyHelpers::UnregisterParameterFilter(ParameterEditor);
	FMaterialPropertyHelpers::UnregisterCustomParameterEditor(ParameterEditor);
	ParameterEditor.Reset();

	UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.Remove(OnGetExtraObjectTagsDelegateHandle);
	
	FCoreDelegates::OnPostEngineInit.Remove(OnPostEngineInitDelegateHandle);

	if (GEditor)
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.Remove(OnAssetPostImportDelegateHandle);
	}

	if (UObjectInitialized())
	{
		UThumbnailManager::Get().UnregisterCustomRenderer(UTextureSet::StaticClass());
	}
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

		for (const TSharedRef<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action);
		}

		RegisteredAssetTypeActions.Empty();
	}
}

void FTextureSetsEditorModule::RegisterCustomizations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomPropertyTypeLayout(FTextureSetAssetParamsCollectionCustomization::GetPropertyTypeName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTextureSetAssetParamsCollectionCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FTextureSetSourceTextureReferenceCustomization::GetPropertyTypeName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTextureSetSourceTextureReferenceCustomization::MakeInstance));
}

void FTextureSetsEditorModule::UnregisterCustomizations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.UnregisterCustomPropertyTypeLayout(FTextureSetAssetParamsCollectionCustomization::GetPropertyTypeName());
	PropertyModule.UnregisterCustomPropertyTypeLayout(FTextureSetSourceTextureReferenceCustomization::GetPropertyTypeName());
}

void FTextureSetsEditorModule::OnGetExtraObjectTagsWithContext(FAssetRegistryTagsContext Context)
{
	if (const UTexture* Texture = Cast<UTexture>(Context.GetObject()))
	{
		// Add a string with the ID of our source texture to the asset data, so it can be checked for change without having to deserialize the whole asset.
		FString IdString;
		if (TextureSetsHelpers::GetSourceDataIdAsString(Texture, IdString))
		{
			Context.AddTag(UObject::FAssetRegistryTag(TextureSetsHelpers::TextureBulkDataIdAssetTagName, IdString, UObject::FAssetRegistryTag::TT_Hidden));
		}
	}
}

void FTextureSetsEditorModule::OnAssetPostImport(UFactory* ImportFactory, UObject* InObject)
{
	if (IsValid(InObject) && InObject->IsA<UTexture>())
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		TArray<FAssetDependency> Referencers;
		AssetRegistry.GetReferencers(InObject->GetPackage()->GetFName(), Referencers);

		for (const FAssetDependency& Dep : Referencers)
		{
			TArray<FAssetData> AssetsInPackage;
			AssetRegistry.GetAssetsByPackageName(Dep.AssetId.PackageName, AssetsInPackage, false);

			for (const FAssetData& AssetData : AssetsInPackage)
			{
				if (AssetData.IsAssetLoaded() && AssetData.IsInstanceOf(UTextureSet::StaticClass()))
				{
					UTextureSet* TS = Cast<UTextureSet>(AssetData.GetAsset());
					TS->UpdateDerivedData(true, true);
				}
			}
		}
	}
}

void FTextureSetsEditorModule::FixupTextureSetData()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
	const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UMaterialInstance::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UMaterialFunction::StaticClass()->GetClassPathName());

	AssetRegistry.EnumerateAssets(Filter, [&AssetRegistry](const FAssetData& AssetData)
	{
		TArray<FAssetIdentifier> Dependencies;

		AssetRegistry.GetDependencies(AssetData.PackageName, Dependencies);

		bool bMarkedDirty = false;

		for (FAssetIdentifier DependentPackage : Dependencies)
		{
			TArray<FAssetData> DependentAssets;
			AssetRegistry.GetAssetsByPackageName(DependentPackage.PackageName, DependentAssets);

			for (FAssetData Dep : DependentAssets)
			{
				if (Dep.AssetClassPath == UTextureSet::StaticClass()->GetClassPathName()
					|| Dep.AssetClassPath == UTextureSetDefinition::StaticClass()->GetClassPathName())
				{
					AssetData.GetPackage()->MarkPackageDirty();
					bMarkedDirty = true;
					break;
				}
			}

			if (bMarkedDirty)
				break;
		}

		return true;
	});
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTextureSetsEditorModule, TextureSets)
