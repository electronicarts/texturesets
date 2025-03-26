// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "ProcessingNodes/TextureOperator.h"

class FTextureOperatorCombineVariance : public FTextureOperator
{
public:
	FTextureOperatorCombineVariance(TSharedRef<ITextureProcessingNode> I, TSharedRef<ITextureProcessingNode> N) : FTextureOperator(I)
		, bPrepared(false)
		, Normals(N)
	{}

	virtual FName GetNodeTypeName() const  { return "CombineVariance v6"; }

	virtual void ComputeGraphHash(FHashBuilder& HashBuilder) const override;
	virtual void ComputeDataHash(const FTextureSetProcessingContext& Context, FHashBuilder& HashBuilder) const override;
	void Prepare(const FTextureSetProcessingContext& Context) override;
	virtual void Cache() override;
	void WriteChannel(int32 Channel, int32 Mip, const FTextureDataTileDesc& Tile, float* TextureData) const override;

private:
	bool bPrepared;
	bool bDisableNormalToRoughness;
	FTextureSetProcessedTextureDef RoughnessDef;
	FTextureDimension RoughnessDim;
	TSharedRef<ITextureProcessingNode> Normals;
};