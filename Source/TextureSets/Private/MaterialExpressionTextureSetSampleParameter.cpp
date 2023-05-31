// (c) Electronic Arts. All Rights Reserved.

#include "MaterialExpressionTextureSetSampleParameter.h"

#include "TextureSetMaterialGraphBuilder.h"
#include "MaterialPropertyHelpers.h"
#include "IMaterialEditor.h"
#include "MaterialCompiler.h"
#include "MaterialEditorUtilities.h"
#include "MaterialHLSLGenerator.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "HLSLTree/HLSLTreeCommon.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"

UMaterialExpressionTextureSetSampleParameter::UMaterialExpressionTextureSetSampleParameter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bShowOutputNameOnPin = true;
	bShowOutputs         = true;
}

FName UMaterialExpressionTextureSetSampleParameter::GetTextureParameterName(int TextureIndex) const
{
	return FName(ParameterName.ToString() + "_PACKED_" + FString::FromInt(TextureIndex));
}

UMaterialFunction* UMaterialExpressionTextureSetSampleParameter::CreateMaterialFunction()
{
	if (!IsValid(Definition))
		return nullptr;

	FTextureSetMaterialGraphBuilder GraphBuilder = FTextureSetMaterialGraphBuilder(Definition, this);

	// Call out to modules to do the work of connecting processed texture samples to outputs
	Definition->GenerateSamplingGraph(this, GraphBuilder);

	// TODO: What's this, does it need to be called?
	UpdateSampleParamArray();

	return GraphBuilder.GetMaterialFunction();
}

void UMaterialExpressionTextureSetSampleParameter::UpdateSampleParamArray()
{
	if (!IsValid(Definition))
		return;

	TArray<TSubclassOf<UTextureSetSampleParams>> RequiredSampleParamClasses = Definition->GetRequiredSampleParamClasses();
	TArray<TSubclassOf<UTextureSetSampleParams>> ExistingSampleParamClasses;

	// Remove un-needed sample params
	for (int i = 0; i < SampleParams.Num(); i++)
	{
		UTextureSetSampleParams* SampleParam = SampleParams[i];
		if (!RequiredSampleParamClasses.Contains(SampleParam->StaticClass()))
		{
			SampleParams.RemoveAt(i);
			i--;
		}
		else
		{
			ExistingSampleParamClasses.Add(SampleParam->StaticClass());
		}
	}

	// Add missing sample params
	for (TSubclassOf<UTextureSetSampleParams> SampleParamClass : RequiredSampleParamClasses)
	{
		if (!ExistingSampleParamClasses.Contains(SampleParamClass))
		{
			SampleParams.Add(NewObject<UTextureSetSampleParams>(this, SampleParamClass));
		}
	}
}