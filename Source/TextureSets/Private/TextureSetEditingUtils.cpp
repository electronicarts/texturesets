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
#include "TextureSetDefinition.h"
#include "TextureSetAssetUserData.h"

TArray<FName> FTextureSetEditingUtils::FindReferencers(const FName PackageName)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FName> HardDependencies;
	AssetRegistryModule.Get().GetReferencers(PackageName, HardDependencies);
	return HardDependencies;
}

const UMaterialExpressionTextureSetSampleParameter* FTextureSetEditingUtils::FindSampleExpression(const FGuid& NodeID, UMaterial* Material)
{
	// FindExpressionByGUID() doesn't work because it ignores subclasses of material function calls. We need to re-implement a search.

	TArray<const UMaterialExpressionTextureSetSampleParameter*> SamplerExpressions;
	Material->GetAllExpressionsOfType<UMaterialExpressionTextureSetSampleParameter>(SamplerExpressions);
	
	for (const UMaterialExpressionTextureSetSampleParameter* CurNode : SamplerExpressions)
	{
		if (CurNode->MaterialExpressionGuid == NodeID)
			return CurNode;
	}
	return nullptr;
}
