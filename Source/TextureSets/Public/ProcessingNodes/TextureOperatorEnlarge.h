// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "TextureOperator.h"

class FTextureOperatorEnlarge : public FTextureOperator
{
public:
	FTextureOperatorEnlarge(TSharedRef<IProcessingNode> I, int NewWidth, int newHeight) : FTextureOperator(I)
		, Width(NewWidth)
		, Height(newHeight)
	{}

	virtual FName GetNodeTypeName() const  { return "Enlarge"; }

	virtual const uint32 ComputeGraphHash() const override
	{
		uint32 Hash = FTextureOperator::ComputeGraphHash();

		Hash = HashCombine(Hash, GetTypeHash(Width));
		Hash = HashCombine(Hash, GetTypeHash(Height));

		return Hash;
	}

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
#endif
