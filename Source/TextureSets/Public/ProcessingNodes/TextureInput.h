// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageCore.h"
#include "IProcessingNode.h"
#include "TextureSetInfo.h"

class FTextureInput : public IProcessingNode
{
public:
	FTextureInput(FName SourceName, const FTextureSetSourceTextureDef& SourceDefinition);

	virtual void Initialize(const FTextureSetProcessingContext& Context);

	virtual int GetWidth() const override { return Image.GetWidth(); }
	virtual int GetHeight() const override { return Image.GetHeight(); }
	virtual const FTextureSetProcessedTextureDef& GetTextureDef() override { return SourceDefinition; }

	virtual float GetPixel(int X, int Y, int Channel) const override;


private:
	FName SourceName;
	FTextureSetSourceTextureDef SourceDefinition;

	mutable FCriticalSection InitializeCS;

	bool bInitialized;
	uint8 ValidChannels;
	FImage Image;
};
