#include "TextureSetEditingUtils.h"

#include "Editor.h"
#include "EditorSupportDelegates.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialEditorUtilities.h"
#include "MaterialPropertyHelpers.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraph.h"
#include "Materials/MaterialExpressionExecBegin.h"
#include "Materials/MaterialExpressionExecEnd.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "TextureSet.h"
#include "TextureSetAssetUserData.h"

TArray<FName> FTextureSetEditingUtils::FindReferencers(const FName PackageName)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FName> HardDependencies;
	AssetRegistryModule.Get().GetReferencers(PackageName, HardDependencies);
	return HardDependencies;
}


void FTextureSetEditingUtils::UpdateMaterialInstance(UMaterialInstance* MaterialInstancePtr)
{
	if (!MaterialInstancePtr)
		return;

	UTextureSetAssetUserData* tsAssetUserData = MaterialInstancePtr->GetAssetUserDataChecked<UTextureSetAssetUserData>();
	if (!tsAssetUserData)
	{
		return;
	}

	// Ryan: Fix and Optimize me
	for (auto Expression : MaterialInstancePtr->GetMaterial()->GetExpressions())
	{
		if (!Expression->IsA(UMaterialExpressionTextureSetSampleParameter::StaticClass()))
		{
			continue;
		}

		for (auto Override : tsAssetUserData->TexturesSetOverrides)
		{
			if (Expression->MaterialExpressionGuid != Override.Guid) // Make me better
			{
				continue;
			}

			for (int packedTextureIdx = 0; packedTextureIdx < Override.TextureSet->Textures.Num(); packedTextureIdx++)
			{
				FName Name = Cast<UMaterialExpressionTextureObjectParameter>(Expression->GetInput(packedTextureIdx + 1)->Expression)->ParameterName;

				auto OverrideInstance = MaterialInstancePtr->TextureParameterValues.FindByPredicate([Name](const FTextureParameterValue& Param) { return Param.ParameterInfo.Name.IsEqual(Name); });

				UTextureSet* SourceTextureSet = Override.IsOverridden && (Override.TextureSet != nullptr) ? Override.TextureSet : Override.DefaultTextureSet;

				if (!OverrideInstance)
				{
					FTextureParameterValue TextureSetOverride;
					TextureSetOverride.ParameterValue = SourceTextureSet->Textures[packedTextureIdx].TextureAsset;
					TextureSetOverride.ParameterInfo.Name = Name;
					MaterialInstancePtr->TextureParameterValues.Add(TextureSetOverride);
				}
				else
				{
					OverrideInstance->ParameterValue = SourceTextureSet->Textures[packedTextureIdx].TextureAsset;
				}
			}
		}
	}


}

