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
	if (TextureSet)
	{
		Definition = TextureSet->Definition;
		SetMaterialFunctionFromDefinition();
	}

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
	if (TextureSet)
	{
		Definition = TextureSet->Definition;
		SetMaterialFunctionFromDefinition();

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
	if (!TextureSet)
	{
		MaterialFunction = nullptr;
		return;
	}

	check(IsValid(Definition->SamplingMaterialFunction));

	TObjectPtr<UMaterialFunction> NewFunction = DuplicateObject(Definition->SamplingMaterialFunction, this);

	// Set texture parameters
	for (auto& Expression : NewFunction->GetExpressions())
	{
		if (!Expression->IsA(UMaterialExpressionTextureSample::StaticClass()))
		{
			continue;
		}

		for (const auto& TextureInfo : TextureSet->Textures)
		{
			if (Expression->GetParameterName().IsEqual(FName(TextureInfo.TextureName)))
			{
				FMaterialParameterMetadata Meta;
				Expression->GetParameterValue(Meta);
				Meta.Value.Texture = TextureInfo.TextureAsset;
				Expression->SetParameterValue(FName(TextureInfo.TextureName), Meta, EMaterialExpressionSetParameterValueFlags::AssignGroupAndSortPriority);
				break;
			}
		}
	}
	
	FixupMaterialFunction(NewFunction);
	SetMaterialFunction(NewFunction);
}

#undef LOCTEXT_NAMESPACE
