// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IProcessingNode.h"

class FTextureOperator : public ITextureProcessingNode
{
public:
	FTextureOperator(const TSharedRef<ITextureProcessingNode> I)
		: SourceImage(I)
	{}

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override { SourceImage->LoadResources(Context); }
	virtual void Initialize(const FTextureSetProcessingGraph& Graph) override { SourceImage->Initialize(Graph); }

	virtual void ComputeGraphHash(FHashBuilder& HashBuilder) const override
	{
		SourceImage->ComputeGraphHash(HashBuilder);
		HashBuilder << GetNodeTypeName();
	}

	virtual void ComputeDataHash(const FTextureSetProcessingContext& Context, FHashBuilder& HashBuilder) const override { SourceImage->ComputeDataHash(Context, HashBuilder); }

	virtual FTextureDimension GetTextureDimension() const override { return SourceImage->GetTextureDimension(); }
	virtual const FTextureSetProcessedTextureDef GetTextureDef() const override { return SourceImage->GetTextureDef(); }

	const TSharedRef<ITextureProcessingNode> SourceImage;
};
