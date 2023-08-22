// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetProcessingContext.h"

class IProcessingNode
{
public:
	virtual ~IProcessingNode() {}

	virtual void Initialize(const FTextureSetProcessingContext& Context) = 0;

	virtual int GetWidth() const = 0;
	virtual int GetHeight() const = 0;
	virtual const struct FTextureSetProcessedTextureDef& GetTextureDef() = 0;

	virtual float GetPixel(int X, int Y, int Channel) const = 0;
};
