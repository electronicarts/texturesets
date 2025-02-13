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

	virtual const uint32 ComputeGraphHash() const override { return HashCombine(SourceImage->ComputeGraphHash(), GetTypeHash(GetNodeTypeName().ToString())); }
	virtual const uint32 ComputeDataHash(const FTextureSetProcessingContext& Context) const override { return SourceImage->ComputeDataHash(Context); };

	virtual FTextureDimension GetTextureDimension() const override { return SourceImage->GetTextureDimension(); }
	virtual const FTextureSetProcessedTextureDef GetTextureDef() const override { return SourceImage->GetTextureDef(); }

	const TSharedRef<ITextureProcessingNode> SourceImage;
};
