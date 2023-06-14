// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureOperator.h"

class FTextureOperatorEnlarge : public FTextureOperator
{
public:
	FTextureOperatorEnlarge(TSharedRef<ITextureSetTexture> I, int NewWidth, int newHeight) : FTextureOperator(I)
		, Width(NewWidth)
		, Height(newHeight)
	{}

	virtual int GetWidth() const override { return Width; }
	virtual int GetHeight() const override { return Height; }

	virtual float GetPixel(int X, int Y, int Channel) const override
	{
		// TODO: Bilinear interpolation
		return SourceImage->GetPixel(X * (SourceImage->GetWidth() / Width), Y * (SourceImage->GetHeight() / Height), Channel);
	}

private:
	const int Width;
	const int Height;
};
