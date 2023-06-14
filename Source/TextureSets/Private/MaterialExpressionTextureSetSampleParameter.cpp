// (c) Electronic Arts. All Rights Reserved.

#include "MaterialExpressionTextureSetSampleParameter.h"

#include "TextureSetMaterialGraphBuilder.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "HLSLTree/HLSLTreeCommon.h"
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
	return FName("TEXSET_" + ParameterName.ToString() + "_PACKED_" + FString::FromInt(TextureIndex));
}

FName UMaterialExpressionTextureSetSampleParameter::GetConstantParameterName(FName Parameter) const
{
	return FName("TEXSET_" + ParameterName.ToString() + "_" + Parameter.ToString());
}

bool UMaterialExpressionTextureSetSampleParameter::IsTextureSetParameterName(FName Name)
{
	return Name.ToString().StartsWith("TEXSET_", ESearchCase::IgnoreCase);
}

#if WITH_EDITOR
UMaterialFunction* UMaterialExpressionTextureSetSampleParameter::CreateMaterialFunction()
{
	if (!IsValid(Definition))
		return nullptr;

	FTextureSetMaterialGraphBuilder GraphBuilder = FTextureSetMaterialGraphBuilder(this);

	// Call out to modules to do the work of connecting processed texture samples to outputs
	Definition->GenerateSamplingGraph(this, GraphBuilder);

	// TODO: What's this, does it need to be called?
	UpdateSampleParamArray();

	return GraphBuilder.GetMaterialFunction();
}
#endif

#if WITH_EDITOR
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
#endif
