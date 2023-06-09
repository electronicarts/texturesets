// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureOperator.h"

class FTextureOperatorInvert : public FTextureOperator
{
public:
	FTextureOperatorInvert(TSharedRef<ITextureSetTexture> I) : FTextureOperator(I)
	{}

	virtual float GetPixel(int X, int Y, int Channel) const override
	{
		return 1.0f - SourceImage->GetPixel(X, Y, Channel);
	}
};
