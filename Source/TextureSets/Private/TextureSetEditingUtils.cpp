// (c) Electronic Arts. All Rights Reserved.

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

			if (IsValid(Override.TextureSet))
			{
				// TODO: Use cooked textures, not source textures
				for (auto& pair : Override.TextureSet->SourceTextures)
				{
					const FName& Name = pair.Key;
					const TObjectPtr<UTexture> Texture = pair.Value;

					auto OverrideInstance = MaterialInstancePtr->TextureParameterValues.FindByPredicate([Name](const FTextureParameterValue& Param) { return Param.ParameterInfo.Name.IsEqual(Name); });

					if (!OverrideInstance)
					{
						FTextureParameterValue TextureSetOverride;
						TextureSetOverride.ParameterValue = Texture;
						TextureSetOverride.ParameterInfo.Name = Name;
						MaterialInstancePtr->TextureParameterValues.Add(TextureSetOverride);
					}
					else
					{
						OverrideInstance->ParameterValue = Texture;
					}
				}
			}
		}
	}


}

