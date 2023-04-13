// (c) Electronic Arts. All Rights Reserved.


#include "MaterialGraphNode_TSSampler.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "SGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"

extern TAutoConsoleVariable<bool> CVarVisualizeMaterialGraph;

TSharedPtr<SGraphNode> UMaterialGraphNode_TSSampler::CreateVisualWidget()
{
	return SNew(STextureSetSamplerNode, this);
}

void UMaterialGraphNode_TSSampler::DestroyNode()
{
	for (FExpressionInput Input : Cast<UMaterialExpressionTextureSetSampleParameter>(MaterialExpression)->TextureReferenceInputs)
	{
		TObjectPtr<UMaterialExpression> TextureChildNode = Input.Expression;
		if (!TextureChildNode)
		{
			continue;
		}
		TextureChildNode->Modify();
		TextureChildNode->GraphNode->BreakAllNodeLinks();
		MaterialExpression->Material->GetExpressionCollection().RemoveExpression(TextureChildNode);
		MaterialExpression->Material->RemoveExpressionParameter(TextureChildNode);
		
		// Make sure the deleted expression is caught by gc
		TextureChildNode->MarkAsGarbage();
		FBlueprintEditorUtils::RemoveNode(nullptr, TextureChildNode->GraphNode, false);
	}

	Super::DestroyNode();
}

void STextureSetSamplerNode::Construct(const FArguments& InArgs, UMaterialGraphNode* InNode)
{
	this->GraphNode = InNode;
	this->MaterialNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);
	
	this->UpdateGraphNode();
	
	for (const TSharedRef<SGraphPin, ESPMode::ThreadSafe> InputPin : InputPins)
	{
		if (!InputPin->GetPinObj()->GetName().Contains("UV"))
		{
			// Collapsed is the visibility that keeps the pin zeroed in layout
			if (!CVarVisualizeMaterialGraph.GetValueOnAnyThread())
			{
				InputPin->SetVisibility(EVisibility::Collapsed);
			}
		}
	}
}