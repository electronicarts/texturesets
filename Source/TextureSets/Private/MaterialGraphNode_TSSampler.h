// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraphNode_TSSampler.generated.h"

/**
 * 
 */
UCLASS()
class UMaterialGraphNode_TSSampler : public UMaterialGraphNode
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	virtual void DestroyNode() override;
	
};

class STextureSetSamplerNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(STextureSetSamplerNode){}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, UMaterialGraphNode* InNode);
	
	/** Cached material graph node pointer to avoid casting */
	UMaterialGraphNode* MaterialNode = nullptr;

};