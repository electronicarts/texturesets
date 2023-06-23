//
// (c) Electronic Arts.  All Rights Reserved.
//

#include "AssetTypeActions_TextureSet.h"

#include "ContentBrowserModule.h"
#include "EditorSupportDelegates.h"
#include "FileHelpers.h"
#include "IContentBrowserSingleton.h"
#include "MaterialEditingLibrary.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "TextureSet.h"
#include "TextureSetsAssetTypeActionsCommon.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/ScopedSlowTask.h"
#include "Engine/Classes/Materials/MaterialInstance.h"
#include "TextureSetCooker.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

DEFINE_LOG_CATEGORY_STATIC(LogTextureSetFixMaterial, Log, Log);

namespace
{
	void FindAllMaterials(UTextureSet* TextureSet, TArray<TObjectPtr<UMaterial>>& OutMaterials, TArray<TObjectPtr<UMaterialFunctionInterface>>& OutFunctions)
	{
		TArray<TObjectPtr<UMaterialInterface>> MaterialInterfaces;
		TextureSetsAssetTypeActionsCommon::GetReferencersOfType(TextureSet, MaterialInterfaces);

		for (UMaterialInterface* MaterialInterface : MaterialInterfaces)
		{
			UMaterial* Material = Cast<UMaterial>(MaterialInterface);

			UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MaterialInterface);
			if (MaterialInstance != nullptr)
			{
				Material = MaterialInstance->GetMaterial();
			}

			OutMaterials.AddUnique(Material);
		}

		TextureSetsAssetTypeActionsCommon::GetReferencersOfType(TextureSet, OutFunctions);
	}
}

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

bool FAssetTypeActions_TextureSet::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

void FAssetTypeActions_TextureSet::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	if (InObjects.Num() == 1)
	{
		TArray<TWeakObjectPtr<UTextureSet>> TextureSets = GetTypedWeakObjectPtrs<UTextureSet>(InObjects);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("TextureSet_FindMaterials", "Find Materials Using This"),
			LOCTEXT("TextureSet_FindMaterialsTooltip", "Finds all materials that use this texture in the content browser."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_TextureSet::ExecuteFindMaterials, TextureSets[0]),
				FCanExecuteAction()
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("TextureSet_FixMaterialUsage", "Fix Material Usage"),
			LOCTEXT("TextureSet_FixMaterialUsageTooltip", "Find materials using this texture set and fixup."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_TextureSet::ExecuteFixUsages, TextureSets[0]),
				FCanExecuteAction()
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("TextureSet_ForceReCook", "Force Re-Cook"),
			LOCTEXT("TextureSet_ForceReCookTooltip", "Force a re-cook of this texture set in-editor."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_TextureSet::ExecuteForceReCook, TextureSets[0]),
				FCanExecuteAction()
			)
		);
	}

}

void FAssetTypeActions_TextureSet::ExecuteFindMaterials(TWeakObjectPtr<UTextureSet> Object)
{
	TArray<FAssetData> Materials;

	UTextureSet* TextureSet = Object.Get();
	if (TextureSet != nullptr)
	{
		TextureSetsAssetTypeActionsCommon::GetReferencersData(TextureSet, UMaterialInterface::StaticClass(), Materials);
		TextureSetsAssetTypeActionsCommon::GetReferencersData(TextureSet, UMaterialFunction::StaticClass(), Materials);
	}

	if (Materials.Num() > 0)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(Materials);
	}
}

void FAssetTypeActions_TextureSet::ExecuteFixUsages(TWeakObjectPtr<UTextureSet> Object)
{

	TObjectPtr<UTextureSet> TextureSet = Object.Get();
	if (TextureSet != nullptr)
	{
		TArray<TObjectPtr<UPackage>> PackagesToSave = FixMaterialUsage(TextureSet);
		if (PackagesToSave.Num())
		{
			FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, true);
		}

		FEditorDelegates::RefreshEditor.Broadcast();
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	}
}

void FAssetTypeActions_TextureSet::ExecuteForceReCook(TWeakObjectPtr<UTextureSet> Object)
{
	TObjectPtr<UTextureSet> TextureSet = Object.Get();
	if (TextureSet != nullptr)
	{
		TextureSet->UpdateDerivedData();

		FEditorDelegates::RefreshEditor.Broadcast();
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	}
}

TArray<TObjectPtr<UPackage>> FAssetTypeActions_TextureSet::FixMaterialUsage(TObjectPtr<UTextureSet> TextureSet)
{
	TArray<TObjectPtr<UPackage>> PackagesToSave;

	{
		UE_LOG(LogTextureSetFixMaterial, Log, TEXT("Begin fix material usage for '%s' ..."), *TextureSet->GetName());

		TArray<TObjectPtr<UMaterial>> Materials;
		TArray<TObjectPtr<UMaterialFunctionInterface>> Functions;
		FindAllMaterials(TextureSet, Materials, Functions);

		int32 TaskCount = Materials.Num() + Functions.Num();
		FScopedSlowTask Task(TaskCount, LOCTEXT("RuntimeVirtualTexture_FixMaterialUsageProgress", "Fixing materials for Runtime Virtual Texture usage..."));
		Task.MakeDialog();

		for (UMaterial* Material : Materials)
		{
			Task.EnterProgressFrame();

			bool bMaterialModified = false;
			for (UMaterialExpression* Expression : Material->GetExpressions())
			{
				UMaterialExpressionTextureSetSampleParameter* TSSampleExpression = Cast<UMaterialExpressionTextureSetSampleParameter>(Expression);
				if (TSSampleExpression)
				{
					if (TextureSet == TSSampleExpression->DefaultTextureSet)
					{
						Expression->Modify();

						FPropertyChangedEvent Event(UMaterialExpressionTextureSetSampleParameter::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMaterialExpressionTextureSetSampleParameter, DefaultTextureSet)));
						Expression->PostEditChangeProperty(Event);

						bMaterialModified = true;
					}
				}
			}

			if (bMaterialModified)
			{
				UE_LOG(LogTextureSetFixMaterial, Log, TEXT("  Recompile material '%s' ..."), *Material->GetName());

				FScopedSlowTask CompileTask(1, FText::AsCultureInvariant(Material->GetName()));
				CompileTask.MakeDialog();
				CompileTask.EnterProgressFrame();

				UMaterialEditingLibrary::RecompileMaterial(Material);

				PackagesToSave.Add(Material->GetOutermost());
			}
		}

		for (UMaterialFunctionInterface* Function : Functions)
		{
			Task.EnterProgressFrame();

			bool bFunctionModified = false;
			for (const TObjectPtr<UMaterialExpression>& Expression : Function->GetExpressions())
			{
				UMaterialExpressionTextureSetSampleParameter* TSSampleExpression = Cast<UMaterialExpressionTextureSetSampleParameter>(Expression);
				if (TSSampleExpression)
				{
					if (TextureSet == TSSampleExpression->DefaultTextureSet)
					{
						//if (TSSampleExpression->InitVirtualTextureDependentSettings())
						Expression->Modify();

						FPropertyChangedEvent Event(UMaterialExpressionTextureSetSampleParameter::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMaterialExpressionTextureSetSampleParameter, DefaultTextureSet)));
						Expression->PostEditChangeProperty(Event);

						bFunctionModified = true;
						
					}
				}
			}

			if (bFunctionModified)
			{
				UE_LOG(LogTextureSetFixMaterial, Log, TEXT("  Update function '%s' ..."), *Function->GetName());

				FScopedSlowTask CompileTask(1, FText::AsCultureInvariant(Function->GetName()));
				CompileTask.MakeDialog();
				CompileTask.EnterProgressFrame();

				UMaterialEditingLibrary::UpdateMaterialFunction(Function, nullptr);

				PackagesToSave.Add(Function->GetOutermost());
			}
		}

		UE_LOG(LogTextureSetFixMaterial, Log, TEXT("End fix material usage for '%s' ..."), *TextureSet->GetName());
	}

	return PackagesToSave;
}


#undef LOCTEXT_NAMESPACE
