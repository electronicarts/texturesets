// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraphNode_Invisible.generated.h"

/**
 * 
 */
UCLASS()
class TEXTURESETS_API UMaterialGraphNode_Invisible : public UMaterialGraphNode
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	
};

class SGraphNodeInvisible : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeInvisible){}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, UMaterialGraphNode* InNode);
	/** Cached material graph node pointer to avoid casting */
	UMaterialGraphNode* MaterialNode = nullptr;
	
	//virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
