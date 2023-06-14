// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageCore.h"
#include "ITextureSetTexture.h"

// These are just here temporarily
class FImageWrapper : public ITextureSetTexture
{
public:
	FImageWrapper(const FImage& I, int Encoding);

	FImage Image;
	int ValidChannels;

	virtual int GetWidth() const override { return Image.GetWidth(); }
	virtual int GetHeight() const override { return Image.GetHeight(); }
	virtual int GetChannels() const override{ return ValidChannels; }

	virtual float GetPixel(int X, int Y, int Channel) const override;
};
