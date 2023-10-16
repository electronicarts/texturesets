// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "TextureOperator.h"

class FTextureOperatorInvert : public FTextureOperator
{
public:
	FTextureOperatorInvert(TSharedRef<ITextureProcessingNode> I) : FTextureOperator(I)
	{}

	virtual FName GetNodeTypeName() const  { return "Invert"; }

	virtual float GetPixel(int X, int Y, int Z, int Channel) const override
	{
		return 1.0f - SourceImage->GetPixel(X, Y, Z, Channel);
	}
};
#endif
