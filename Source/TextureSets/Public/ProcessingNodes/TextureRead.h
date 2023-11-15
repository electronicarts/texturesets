// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "ImageCore.h"
#include "IProcessingNode.h"
#include "TextureSetInfo.h"
#include "TextureSetProcessingGraph.h"

class UTexture;
class FTextureOperator;

class FTextureRead : public ITextureProcessingNode
{
public:
	FTextureRead(FName SourceName, const FTextureSetSourceTextureDef& SourceDefinition);

	virtual FName GetNodeTypeName() const  { return "TextureRead"; }

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override;

	virtual void Initialize(const FTextureSetProcessingGraph& Graph) override;

	virtual const uint32 ComputeGraphHash() const override;
	virtual const uint32 ComputeDataHash(const FTextureSetProcessingContext& Context) const override;

	virtual int GetWidth() const override { check(bInitialized); return Width; }
	virtual int GetHeight() const override { check(bInitialized); return Height; }
	virtual int GetSlices() const override { check(bInitialized); return Slices; }
	virtual const FTextureSetProcessedTextureDef GetTextureDef() override { return SourceDefinition; }

#if PROCESSING_METHOD == PROCESSING_METHOD_CHUNK
	virtual void ComputeChunk(const FTextureProcessingChunk& Chunk, float* TextureData) const override;
#else
	virtual float GetPixel(int X, int Y, int Z, int Channel) const override;
#endif

	void AddOperator(CreateOperatorFunc Operator) { CreateOperatorFuncs.Add(Operator); }

private:
	FName SourceName;
	FTextureSetSourceTextureDef SourceDefinition;

	TArray<CreateOperatorFunc> CreateOperatorFuncs;
	TArray<TSharedRef<class ITextureProcessingNode>> Operators;

	mutable FCriticalSection InitializeCS;

	TObjectPtr<UTexture> Texture;
	bool bInitialized;
	uint8 ValidChannels;
	int32 ChannelMask;
	uint8 ChannelSwizzle[4];
	int Width;
	int Height;
	int Slices;
	FImage Image;
	bool bValidImage;
};
#endif
