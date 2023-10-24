// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "TextureOperator.h"

class FTextureOperatorEnlarge : public FTextureOperator
{
public:
	FTextureOperatorEnlarge(TSharedRef<ITextureProcessingNode> I, int NewWidth, int NewHeight, int NewSlices) : FTextureOperator(I)
		, Width(NewWidth)
		, Height(NewHeight)
		, Slices(NewSlices)
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

	virtual float GetPixel(int X, int Y, int Z, int Channel) const override
	{
		// TODO: Bilinear/Trilinear interpolation
		const int SourceX = X * (SourceImage->GetWidth() / Width);
		const int SourceY = Y * (SourceImage->GetHeight() / Height);
		const int SourceZ = Z * (SourceImage->GetSlices() / Slices);

		return SourceImage->GetPixel(SourceX, SourceY, SourceZ, Channel);
	}

private:
	const int Width;
	const int Height;
	const int Slices;
};
#endif
