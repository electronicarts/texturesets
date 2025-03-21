// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureOperator.h"

class FTextureOperatorMipChain : public FTextureOperator
{
public:
	FTextureOperatorMipChain(TSharedRef<ITextureProcessingNode> I) : FTextureOperator(I)
		, bPrepared(false)
		, FirstCachedMip(-1)
		, NumMipsToCache(-1)
		, bCached(false)
	{}

	virtual FName GetNodeTypeName() const  { return "MipChain v1"; }

	virtual void ComputeGraphHash(FHashBuilder& HashBuilder) const override
	{
		FTextureOperator::ComputeGraphHash(HashBuilder);
	}

	virtual FTextureDimension GetTextureDimension() const override;

	virtual void Prepare(const FTextureSetProcessingContext& Context) override;

	virtual void Cache() override;

	virtual void WriteChannel(int32 Channel, int32 Mip, const FTextureDataTileDesc& Tile, float* TextureData) const override;

private:
	bool bPrepared;
	FTextureSetProcessedTextureDef SourceDef;
	FTextureDimension SourceDimension;
	FTextureDimension OurDim;
	int32 FirstCachedMip;
	int32 NumMipsToCache;
	TArray<FIntVector3> MipSizes;

	mutable FCriticalSection CacheCS;
	TArray<TArray<float>> CachedMips;

	bool bCached;
};
