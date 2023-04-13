// (c) Electronic Arts. All Rights Reserved.


#include "MaterialGraphNode_Invisible.h"

extern TAutoConsoleVariable<bool> CVarVisualizeMaterialGraph;

TSharedPtr<SGraphNode> UMaterialGraphNode_Invisible::CreateVisualWidget()
{
	return SNew(SGraphNodeInvisible, this);
}

void SGraphNodeInvisible::Construct(const FArguments& InArgs, UMaterialGraphNode* InNode)
{
	this->GraphNode = InNode;
	this->MaterialNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();

	if (!CVarVisualizeMaterialGraph.GetValueOnAnyThread())
	{
		SetVisibility(EVisibility::Hidden);
	}
}