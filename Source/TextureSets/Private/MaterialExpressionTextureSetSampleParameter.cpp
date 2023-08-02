// (c) Electronic Arts. All Rights Reserved.

#include "MaterialExpressionTextureSetSampleParameter.h"

#include "TextureSetMaterialGraphBuilder.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "HLSLTree/HLSLTreeCommon.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

UMaterialExpressionTextureSetSampleParameter::UMaterialExpressionTextureSetSampleParameter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bShowOutputNameOnPin = true;
	bShowOutputs         = true;
}

FName UMaterialExpressionTextureSetSampleParameter::GetTextureParameterName(int TextureIndex) const
{
	return MakeTextureParameterName(ParameterName, TextureIndex);
}

FName UMaterialExpressionTextureSetSampleParameter::GetConstantParameterName(FName ConstantName) const
{
	return MakeConstantParameterName(ParameterName, ConstantName);
}

FName UMaterialExpressionTextureSetSampleParameter::MakeTextureParameterName(FName ParameterName, int TextureIndex)
{
	return FName("TEXSET_" + ParameterName.ToString() + "_PACKED_" + FString::FromInt(TextureIndex));
}

FName UMaterialExpressionTextureSetSampleParameter::MakeConstantParameterName(FName ParameterName, FName ConstantName)
{
	return FName("TEXSET_" + ParameterName.ToString() + "_" + ConstantName.ToString());
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

	// Make sure our samping parameters are up to date before generating the sampling graph
	UpdateSampleParamArray();

	FTextureSetMaterialGraphBuilder GraphBuilder = FTextureSetMaterialGraphBuilder(this);

	// Call out to modules to do the work of connecting processed texture samples to outputs
	Definition->GenerateSamplingGraph(this, GraphBuilder);

	return GraphBuilder.GetMaterialFunction();
}
#endif

#if WITH_EDITOR
EDataValidationResult UMaterialExpressionTextureSetSampleParameter::IsDataValid(FDataValidationContext& Context)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!IsValid(Definition))
	{
		Context.AddError(LOCTEXT("MissingDefinition","A texture set sample must reference a valid definition."));
		Result = EDataValidationResult::Invalid;
	}

	return CombineDataValidationResults(Result, Super::IsDataValid(Context));
}
#endif

#if WITH_EDITOR
void UMaterialExpressionTextureSetSampleParameter::UpdateSampleParamArray()
{
	if (!IsValid(Definition))
		return;

	TArray<TSubclassOf<UTextureSetSampleParams>> RequiredSampleParamClasses = Definition->GetRequiredSampleParamClasses();

	// Remove un-needed sample params
	for (int i = 0; i < SampleParams.Num(); i++)
	{
		UClass* SampleParamClass = SampleParams[i]->GetClass();
		if (RequiredSampleParamClasses.Contains(SampleParamClass))
		{
			RequiredSampleParamClasses.Remove(SampleParamClass); // Remove this sample param from required array, so duplicates will be removed
		}
		else
		{
			SampleParams.RemoveAt(i);
			i--;
		}
	}

	// Add missing sample params
	for (TSubclassOf<UTextureSetSampleParams> SampleParamClass : RequiredSampleParamClasses)
	{
		SampleParams.Add(NewObject<UTextureSetSampleParams>(this, SampleParamClass));
	}
}
#endif

#undef LOCTEXT_NAMESPACE
