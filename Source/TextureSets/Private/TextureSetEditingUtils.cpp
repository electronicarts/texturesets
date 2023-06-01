﻿// (c) Electronic Arts. All Rights Reserved.

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
#include "TextureSetDefinition.h"
#include "TextureSetAssetUserData.h"

TArray<FName> FTextureSetEditingUtils::FindReferencers(const FName PackageName)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FName> HardDependencies;
	AssetRegistryModule.Get().GetReferencers(PackageName, HardDependencies);
	return HardDependencies;
}

const UMaterialExpressionTextureSetSampleParameter* FTextureSetEditingUtils::FindSampleExpression(const FSetOverride& TextureSetOverride, UMaterial* Material)
{
	TArray<const UMaterialExpressionTextureSetSampleParameter*> SamplerExpressions;
	Material->GetAllExpressionsOfType<UMaterialExpressionTextureSetSampleParameter>(SamplerExpressions);
	
	const TArray<UMaterialExpression*>* ExpressionList = Material->EditorParameters.Find(TextureSetOverride.Name);
	for (const UMaterialExpressionTextureSetSampleParameter* CurNode : SamplerExpressions)
	{
		if (CurNode->ParameterName == TextureSetOverride.Name || CurNode->MaterialExpressionGuid == TextureSetOverride.MaterialExpressionGuid)
		{
			return CurNode;
		}
	}
	return nullptr;
}
