#pragma once
#include "MaterialGraph/MaterialGraphNode.h"

class FTextureSetEditingUtils
{
public:
	template <class NodeType>
	static UMaterialGraphNode* InitExpressionNewNode(UMaterialGraph* Graph, UMaterialExpression* Expression,
	                                                 bool bUserInvoked);

	static TArray<FName> FindReferencers(FName PackageName);
};
