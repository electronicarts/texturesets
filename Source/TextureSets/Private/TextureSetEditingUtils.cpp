#include "TextureSetEditingUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialGraph/MaterialGraphNode.h"


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


TArray<FName> FTextureSetEditingUtils::FindReferencers(const FName PackageName)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FName> HardDependencies;
	AssetRegistryModule.Get().GetReferencers(PackageName, HardDependencies);
	return HardDependencies;
}
