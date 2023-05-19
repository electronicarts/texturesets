//
// (c) Electronic Arts.  All Rights Reserved.
//
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialPropertyHelpers.h"

#include "IMaterialEditor.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "MaterialCompiler.h"
#include "MaterialEditingLibrary.h"
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

#define LOCTEXT_NAMESPACE "FTextureSetsModule"

UMaterialExpressionTextureSetSampleParameter::UMaterialExpressionTextureSetSampleParameter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bShowOutputNameOnPin = true;
	bShowOutputs         = true;
}

void UMaterialExpressionTextureSetSampleParameter::PostLoad()
{
	SetMaterialFunctionFromDefinition();
	Super::PostLoad();
}

// -----

#if WITH_EDITOR

void UMaterialExpressionTextureSetSampleParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Texture Set Sample Parameter"));
}

FText UMaterialExpressionTextureSetSampleParameter::GetKeywords() const
{ 
	return FText::FromString(TEXT("+"));
}



bool UMaterialExpressionTextureSetSampleParameter::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	ensureAlwaysMsgf(false, TEXT("GenerateHLSLExpression not implemented for Texture Sets"));
	return false;
}

void UMaterialExpressionTextureSetSampleParameter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		const FName ChangedPropName = PropertyChangedEvent.Property->GetFName();

		if (ChangedPropName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionTextureSetSampleParameter, Definition))
		{
			SetMaterialFunctionFromDefinition();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

// TODO: Borrowed from UCompoundMaterialExpression::UpdateMaterialFunction. Should switch/delete later
void UMaterialExpressionTextureSetSampleParameter::FixupMaterialFunction(TObjectPtr<UMaterialFunction> NewMaterialFunction)
{
	if (!IsValid(NewMaterialFunction))
	{
		return;
	}

	// Fix up Expression input/output references, since while the "FunctionInputs" and "FunctionOutputs" are serialized,
	// the UMaterialFunction is not, and thus the ExpressionInput/Output values are null.
	for (UMaterialExpression* CurrentExpression : NewMaterialFunction->GetExpressions())
	{
		UMaterialExpressionFunctionOutput* OutputExpression = Cast<UMaterialExpressionFunctionOutput>(CurrentExpression);
		UMaterialExpressionFunctionInput* InputExpression = Cast<UMaterialExpressionFunctionInput>(CurrentExpression);

		if (InputExpression)
		{
			for (FFunctionExpressionInput& Input : FunctionInputs)
			{
				if (InputExpression->InputName == Input.Input.InputName)
				{
					Input.ExpressionInput = InputExpression;
				}
			}
		}
		else if (OutputExpression)
		{
			for (FFunctionExpressionOutput& Output : FunctionOutputs)
			{
				if (OutputExpression->OutputName == Output.Output.OutputName)
				{
					Output.ExpressionOutput = OutputExpression;
				}
			}
		}
	}
}

void UMaterialExpressionTextureSetSampleParameter::SetMaterialFunctionFromDefinition()
{
	if (!IsValid(Definition))
	{
		SetMaterialFunction(nullptr);
		return;
	}

	TObjectPtr<UMaterialFunction> NewFunction = Definition->GenerateMaterialFunction(this);
	check(IsValid(NewFunction));
	
	FixupMaterialFunction(NewFunction);
	SetMaterialFunction(NewFunction);
	UpdateSampleParamArray();
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

#undef LOCTEXT_NAMESPACE
