#pragma once
#include "MaterialGraph/MaterialGraphNode.h"

class FTextureSetEditingUtils
{
public:
	template <class NodeType>
	static UMaterialGraphNode* InitExpressionNewNode(UMaterialGraph* Graph, UMaterialExpression* Expression,
	                                                 bool bUserInvoked);

	static TArray<FName> FindReferencers(FName PackageName);

	static void UpdateMaterialInstance(UMaterialInstance* MaterialInstancePtr);
};


template<typename NodeType>
UMaterialGraphNode* FTextureSetEditingUtils::InitExpressionNewNode(UMaterialGraph* Graph, UMaterialExpression* Expression, bool bUserInvoked)
{
	UMaterialGraphNode* NewNode = nullptr;

	FGraphNodeCreator<NodeType> NodeCreator(*Graph);
	if (bUserInvoked)
	{
		NewNode = NodeCreator.CreateUserInvokedNode();
	}
	else
	{
		NewNode = NodeCreator.CreateNode(false);
	}
	NewNode->MaterialExpression = Expression;
	NewNode->RealtimeDelegate = Graph->RealtimeDelegate;
	NewNode->MaterialDirtyDelegate = Graph->MaterialDirtyDelegate;
	Expression->GraphNode = NewNode;
	Expression->SubgraphExpression = Graph->SubgraphExpression;
	NodeCreator.Finalize();

	return NewNode;
}

