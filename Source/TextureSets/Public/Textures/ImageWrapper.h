// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ITextureSetTexture.h"

// These are just here temporarily
class FImageWrapper : public ITextureSetTexture
{
public:
	FImageWrapper(TSharedRef<FImage> I) : Image(I)
	{}

	TSharedRef<FImage> Image;

	virtual int GetWidth() const override { return Image->GetWidth(); }
	virtual int GetHeight() const override { return Image->GetHeight(); }
	virtual int GetChannels() const override{ return 3; } // TODO

	virtual float GetPixel(int X, int Y, int Channel) const override
	{
		// TODO: Support more than just RGBA32F textures
		return Image->AsRGBA32F()[X * GetWidth() + Y].Component(Channel);
	}
};
