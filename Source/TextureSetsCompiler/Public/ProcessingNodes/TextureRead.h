// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageCore.h"
#include "IProcessingNode.h"
#include "TextureSetInfo.h"
#include "TextureSetProcessingGraph.h"
#include "TextureSetsHelpers.h"

class UTexture;
class FTextureOperator;

#define DIRECT_READ 1

class FTextureRead : public ITextureProcessingNode
{
public:
	FTextureRead(FName SourceName, const FTextureSetSourceTextureDef& SourceDefinition);

	virtual FName GetNodeTypeName() const  { return "TextureRead"; }

	virtual void ComputeGraphHash(FHashBuilder& HashBuilder) const override;
	virtual void ComputeDataHash(const FTextureSetProcessingContext& Context, FHashBuilder& HashBuilder) const override;
	virtual void Prepare(const FTextureSetProcessingContext& Context) override;
	virtual void Cache() override;

	virtual FTextureDimension GetTextureDimension() const override { check(bPrepared); return { Width, Height, Slices }; }
	virtual const FTextureSetProcessedTextureDef GetTextureDef() const override { return SourceDefinition; }

	virtual void WriteChannel(int32 Channel, const FTextureDataTileDesc& Tile, float* TextureData) const override;

	void AddOperator(CreateOperatorFunc Operator) { CreateOperatorFuncs.Add(Operator); }

private:
	FName SourceName;
	FTextureSetSourceTextureDef SourceDefinition;

	TArray<CreateOperatorFunc> CreateOperatorFuncs;
	TArray<TSharedRef<class ITextureProcessingNode>> Operators;

	bool bPrepared;
	FTextureSource AsyncSource;

	uint8 ValidChannels;
	int32 ChannelMask;
	uint8 ChannelSwizzle[4];
	int Width;
	int Height;
	int Slices;
	ETextureSourceFormat TextureSourceFormat;
	EGammaSpace TextureSourceGamma;
	FSharedBuffer TextureSourceMip0;
};
