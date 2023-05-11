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