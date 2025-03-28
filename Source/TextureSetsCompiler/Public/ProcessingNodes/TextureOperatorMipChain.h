// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureOperator.h"

class TEXTURESETSCOMPILER_API FTextureOperatorMipChain : public FTextureOperator
{
public:
	FTextureOperatorMipChain(TSharedRef<ITextureProcessingNode> I) : FTextureOperator(I)
		, bPrepared(false)
		, FirstCachedMip(-1)
		, NumMipsToCache(-1)
		, bCached(false)
	{}

	virtual FName GetNodeTypeName() const  { return "MipChain v2"; }

	virtual void ComputeGraphHash(FHashBuilder& HashBuilder) const override
	{
		FTextureOperator::ComputeGraphHash(HashBuilder);
	}

	virtual FTextureDimension GetTextureDimension() const override;
	virtual void Prepare(const FTextureSetProcessingContext& Context) override;
	virtual void Cache() override;
	virtual void WriteChannel(int32 Channel, int32 Mip, const FTextureDataTileDesc& Tile, float* TextureData) const override;

private:
	static void ComputeMip(
		FIntVector3 SourceSize, float* SourceData,
		FIntVector3 DestSize, float* DestData,
		uint8 Channels, bool bIsVolume);

	bool bPrepared;
	FTextureSetProcessedTextureDef SourceDef;
	FTextureDimension SourceDimension;
	FTextureDimension OurDim;
	int32 FirstCachedMip;
	int32 NumMipsToCache;

	mutable FCriticalSection CacheCS;
	TArray<TArray<float>> CachedMips;

	bool bCached;
};
