// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageCore.h"
#include "ITextureSetTexture.h"

// TODO: Rename to FTextureWrapper
class FImageWrapper : public ITextureSetTexture
{
public:
	FImageWrapper(UTexture* Texture);

	mutable FCriticalSection InitializeCS;
	bool Initialized;
	UTexture* Texture;
	FImage Image;
	int ValidChannels;

	virtual void Initialize();

	virtual int GetWidth() const override { return Image.GetWidth(); }
	virtual int GetHeight() const override { return Image.GetHeight(); }
	virtual int GetChannels() const override{ return ValidChannels; }

	virtual float GetPixel(int X, int Y, int Channel) const override;
};
