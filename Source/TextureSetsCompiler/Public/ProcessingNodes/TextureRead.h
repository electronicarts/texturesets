// (c) Electronic Arts. All Rights Reserved.

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

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override;

	virtual void Initialize(const FTextureSetProcessingGraph& Graph) override;

	virtual const uint32 ComputeGraphHash() const override;
	virtual const uint32 ComputeDataHash(const FTextureSetProcessingContext& Context) const override;

	virtual FTextureDimension GetTextureDimension() const override { check(bLoaded); return {Width, Height, Slices}; }
	virtual const FTextureSetProcessedTextureDef GetTextureDef() override { return SourceDefinition; }

	virtual void ComputeChunk(const FTextureProcessingChunk& Chunk, float* TextureData) const override;

	void AddOperator(CreateOperatorFunc Operator) { CreateOperatorFuncs.Add(Operator); }

private:
	FName SourceName;
	FTextureSetSourceTextureDef SourceDefinition;

	TArray<CreateOperatorFunc> CreateOperatorFuncs;
	TArray<TSharedRef<class ITextureProcessingNode>> Operators;

	mutable FCriticalSection InitializeCS;

	TObjectPtr<UTexture> Texture;
	FReferenceHolder TextureReferenceHolder;
	bool bLoaded;
	bool bInitialized;
	uint8 ValidChannels;
	int32 ChannelMask;
	uint8 ChannelSwizzle[4];
	int Width;
	int Height;
	int Slices;
#if DIRECT_READ
	ETextureSourceFormat TextureSourceFormat;
	EGammaSpace TextureSourceGamma;
	FSharedBuffer TextureSourceMip0;
#else
	FImage Image;
#endif
	bool bValidImage;
};
