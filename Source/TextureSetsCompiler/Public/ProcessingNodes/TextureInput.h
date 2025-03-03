// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "IProcessingNode.h"
#include "TextureSetInfo.h"
#include "TextureSetProcessingGraph.h"

class FTextureRead;

class FTextureInput : public ITextureProcessingNode
{
private:
	// Constructor is private so this node can only be created via FTextureSetProcessingGraph::AddInputTexture
	friend class FTextureSetProcessingGraph;
	FTextureInput(FName SourceName, const FTextureSetSourceTextureDef& SourceDefinition);
public:
	virtual FName GetNodeTypeName() const  { return "TextureInput"; }

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override { check(LastNode); LastNode->LoadResources(Context); }

	virtual void Initialize(const FTextureSetProcessingGraph& Graph) override { check(LastNode); LastNode->Initialize(Graph); }

	virtual void ComputeGraphHash(FHashBuilder& HashBuilder) const override;
	virtual void ComputeDataHash(const FTextureSetProcessingContext& Context, FHashBuilder& HashBuilder) const override { check(LastNode); LastNode->ComputeDataHash(Context, HashBuilder); }

	virtual FTextureDimension GetTextureDimension() const override { check(LastNode); return LastNode->GetTextureDimension(); }
	virtual const FTextureSetProcessedTextureDef GetTextureDef() const override { check(LastNode); return LastNode->GetTextureDef(); }

	virtual void ComputeChannel(int32 Channel, const FTextureDataTileDesc& Tile, float* TextureData) const override { LastNode->ComputeChannel(Channel, Tile, TextureData); }

	void AddOperator(CreateOperatorFunc Operator) { CreateOperatorFuncs.Add(Operator); }

private:
	void InstantiateOperators(const FTextureSetProcessingGraph& Graph);

	TArray<CreateOperatorFunc> CreateOperatorFuncs;
	TArray<TSharedRef<class ITextureProcessingNode>> Operators;
	TSharedPtr<ITextureProcessingNode> LastNode;
	TSharedRef<FTextureRead> TextureRead;
};
