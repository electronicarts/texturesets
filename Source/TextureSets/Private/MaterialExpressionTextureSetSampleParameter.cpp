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
#include "MaterialGraphNode_Invisible.h"
#include "MaterialGraphNode_TSSampler.h"
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
		GenerateMaterialFunction(true);
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

void UMaterialExpressionTextureSetSampleParameter::BuildTextureParameterChildren()
{
	if (TextureSet)
	{
		GenerateMaterialFunction();
		TArray<UMaterialExpression*> inputsToDelete;
		// for (auto Input : TextureReferenceInputs)
		// {
		// 	if (Input.Expression)
		// 	{
		// 		inputsToDelete.Add(Input.Expression);
		// 		Input.Expression->Modify();
		// 		if (Input.Expression->GraphNode)
		// 		{
		// 			Input.Expression->GraphNode->BreakAllNodeLinks();
		// 		}
		// 	}
		// }

		int TextureIndex = 0; 
		//for (auto Texture : TextureSetData->Items)

		if (!Definition)
		{
			Definition = TextureSet->Definition;
		}

		for (int i = 0; i < Definition->Items.Num(); i += 1)
		{
			FTextureData Texture = TextureSet->Textures[i];
			/*if (!Texture)
			{
				Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"), nullptr, LOAD_None, nullptr);
			}*/
			Material->Modify();

			if (Material->MaterialGraph)
			{
				Material->MaterialGraph->Modify();


				auto refNode = FMaterialEditorUtilities::CreateNewMaterialExpression(
					Material->MaterialGraph, UMaterialExpressionTextureReferenceInternal::StaticClass(),
					FVector2d(0, 0), false, true);

				refNode->SetParameterName(FName(Texture.TextureName));
				FMaterialParameterMetadata meta;
				meta.Value.Type = EMaterialParameterType::Texture;
				meta.Value.Texture = Texture.TextureAsset ? Texture.TextureAsset : LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"), nullptr, LOAD_None, nullptr);
				meta.Group = FMaterialPropertyHelpers::TextureSetParamName;
				meta.SortPriority = 0;
				refNode->SetParameterValue(FName(Texture.TextureName), meta, EMaterialExpressionSetParameterValueFlags::AssignGroupAndSortPriority);
				refNode->UpdateParameterGuid(true, true);
				Material->AddExpressionParameter(refNode, Material->EditorParameters);

				//TextureReferenceDummies.Add((UMaterialExpressionTextureObjectParameter*)refNode);
				Material->MaterialGraph->GetSchema()->TryCreateConnection(refNode->GraphNode->Pins[3], this->GraphNode->Pins[1 + TextureIndex]);

				TextureIndex += 1;
			}
			else
			{
				auto RefExpression = UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureReferenceInternal::StaticClass());

				RefExpression->SetParameterName(FName(Texture.TextureName));
				FMaterialParameterMetadata meta;
				meta.Value.Type = EMaterialParameterType::Texture;
				meta.Value.Texture = Texture.TextureAsset ? Texture.TextureAsset : LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"), nullptr, LOAD_None, nullptr);
				meta.Group = FMaterialPropertyHelpers::TextureSetParamName;
				meta.SortPriority = 0;
				RefExpression->SetParameterValue(FName(Texture.TextureName), meta, EMaterialExpressionSetParameterValueFlags::AssignGroupAndSortPriority);
				RefExpression->UpdateParameterGuid(true, true);
				Material->AddExpressionParameter(RefExpression, Material->EditorParameters);
				Material->GetExpressionCollection().AddExpression(RefExpression);
				GetInput(TextureIndex + 1)->Connect(0, RefExpression);
				TextureIndex += 1;
			}
		}
		
		for (auto Expression : inputsToDelete)
		{
			Material->GetExpressionCollection().RemoveExpression(Expression);
			Material->RemoveExpressionParameter(Expression);

			// Make sure the deleted expression is caught by gc
			Expression->MarkAsGarbage();
			if (Expression->GraphNode)
			{
				FBlueprintEditorUtils::RemoveNode(NULL, Expression->GraphNode, true);
			}
		}
	}
}

void UMaterialExpressionTextureSetSampleParameter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (TextureSet)
	{
		Definition = TextureSet->Definition;
		GenerateMaterialFunction();

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

	SetMaterialFunction(NewMaterialFunction);
}

void UMaterialExpressionTextureSetSampleParameter::GenerateMaterialFunction(bool CalledFromPostEdit)
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
}

#undef LOCTEXT_NAMESPACE
