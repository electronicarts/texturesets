// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "ImageCore.h"
#include "IProcessingNode.h"
#include "TextureSetInfo.h"

class UTexture;

class FTextureInput : public ITextureProcessingNode
{
public:
	FTextureInput(FName SourceName, const FTextureSetSourceTextureDef& SourceDefinition);

	virtual FName GetNodeTypeName() const  { return "TextureInput"; }

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override;

	virtual void Initialize(const FTextureSetProcessingContext& Context) override;

	virtual const uint32 ComputeGraphHash() const override;
	virtual const uint32 ComputeDataHash(const FTextureSetProcessingContext& Context) const override;

	virtual int GetWidth() const override { return Image.GetWidth(); }
	virtual int GetHeight() const override { return Image.GetHeight(); }
	virtual const FTextureSetProcessedTextureDef& GetTextureDef() override { return SourceDefinition; }

	virtual float GetPixel(int X, int Y, int Channel) const override;


private:
	FName SourceName;
	FTextureSetSourceTextureDef SourceDefinition;

	mutable FCriticalSection InitializeCS;

	TObjectPtr<UTexture> Texture;
	bool bInitialized;
	uint8 ValidChannels;
	uint8 ChannelSwizzle[4];
	FImage Image;
	bool bValidImage;
};
#endif
