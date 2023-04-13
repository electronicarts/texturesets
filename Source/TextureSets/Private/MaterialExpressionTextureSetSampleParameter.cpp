//
// (c) Electronic Arts.  All Rights Reserved.
//
#include "MaterialExpressionTextureSetSampleParameter.h"

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

#define LOCTEXT_NAMESPACE "FTextureSetsModule"

UMaterialExpressionTextureSetSampleParameter::UMaterialExpressionTextureSetSampleParameter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bShowOutputNameOnPin = true;
	bShowOutputs         = true;
	TextureReferenceInputs.Init(FExpressionInput(), 16);
}

// Responsible for input pins type-correctness
uint32 UMaterialExpressionTextureSetSampleParameter::GetInputType(int32 InputIndex)
{
	switch(InputIndex)
	{
	case 0:
		return EMaterialValueType::MCT_Float2;
	default:
		return EMaterialValueType::MCT_Texture;
	}
	
}

FExpressionInput* UMaterialExpressionTextureSetSampleParameter::GetInput(int32 InputIndex)
{
	switch(InputIndex)
	{
	case 0:
		return &Coordinates;
	default:
		return &TextureReferenceInputs[InputIndex - 1];
	}
}

const TArray<FExpressionInput*> UMaterialExpressionTextureSetSampleParameter::GetInputs()
{
	TArray<FExpressionInput*> inputs = Super::GetInputs();

	for (FExpressionInput& input : TextureReferenceInputs)
	{
		inputs.Add(&input);
	}

	return inputs;
}

void UMaterialExpressionTextureSetSampleParameter::PostInitProperties()
{
	SetupOutputs();

	Super::PostInitProperties();
}

void UMaterialExpressionTextureSetSampleParameter::PostLoad()
{
	SetupOutputs();
	BuildTextureParameterChildren();

	Super::PostLoad();
}

void UMaterialExpressionTextureSetSampleParameter::PostEditUndo()
{
	// This material expression will be fully recreated
}

// -----

#if WITH_EDITOR



int32 UMaterialExpressionTextureSetSampleParameter::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!TextureSet)
	{
		return INDEX_NONE;
	}

	// This can happen if a texture is removed from an input element
	if (OutputIndex >= TextureSet->Textures.Num())
	{
		return INDEX_NONE;
	}

	if (!TextureSet->Textures[OutputIndex].TextureAsset)
	{
		return INDEX_NONE;
	}

	if (!TextureReferenceInputs[OutputIndex].Expression)
	{
		return INDEX_NONE;
	}
	
	auto Texture = TextureSet->Textures[OutputIndex].TextureAsset;
	
	int32 TextureReferenceIndex = INDEX_NONE;
	int32 TextureCodeIndex = INDEX_NONE;
	EMaterialSamplerType samplertype;
	TOptional<FName> paramName;
	
	TextureCodeIndex = TextureReferenceInputs[OutputIndex].Compile(Compiler);
	Compiler->GetTextureForExpression(TextureCodeIndex, TextureReferenceIndex, samplertype,paramName);
	FString SamplerTypeError;
	int32 CoordinateIndex = Coordinates.Compile(Compiler);

	return Compiler->TextureSample(
		TextureCodeIndex,
		CoordinateIndex,
		EMaterialSamplerType::SAMPLERTYPE_Color);
}

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

		TArray<UMaterialExpression*> inputsToDelete;
		for (auto Input : TextureReferenceInputs)
		{
			if (Input.Expression)
			{
				inputsToDelete.Add(Input.Expression);
				Input.Expression->Modify();
				if (Input.Expression->GraphNode)
				{
					Input.Expression->GraphNode->BreakAllNodeLinks();
				}
			}
		}

		int TextureIndex = 0; 
		for (auto Texture : TextureSet->Textures)
		{
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
				meta.Value.Texture = Texture.TextureAsset;
				meta.Group = FName("Internal_TextureSet");
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
				meta.Value.Texture = Texture.TextureAsset;
				meta.Group = FName("Internal_TextureSet");
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
	BuildTextureParameterChildren();
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::ValueSet)
	{
		return;
	}
	
	SetupOutputs();
}

#endif

void UMaterialExpressionTextureSetSampleParameter::SetupOutputs()
{
#if WITH_EDITORONLY_DATA

	Outputs.Reset();
	
	if (TextureSetData)
	{
		for (auto& TextureInfo : TextureSetData->Items)
		{
			FExpressionOutput ExpressionOutput(FName(TextureInfo.TextureTypes));
			Outputs.Add(ExpressionOutput);
		}
	}

	if (GraphNode)
		GraphNode->ReconstructNode();

#endif
}

#undef LOCTEXT_NAMESPACE
