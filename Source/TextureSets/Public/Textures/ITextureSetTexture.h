// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ITextureSetTexture
{
public:
	virtual ~ITextureSetTexture() {}

	virtual int GetWidth() const = 0;
	virtual int GetHeight() const = 0;
	virtual int GetChannels() const = 0;
	virtual void Prepare() {}; // Optional for any sub-classes that need to pre-process data before sampling
	virtual float GetPixel(int X, int Y, int Channel) const = 0;
};