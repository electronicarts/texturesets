// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureOperator.h"

class FTextureOperatorEnlarge : public FTextureOperator
{
public:
	FTextureOperatorEnlarge(TSharedRef<ITextureProcessingNode> I, int NewWidth, int NewHeight, int NewSlices) : FTextureOperator(I)
		, TargetWidth(NewWidth)
		, TargetHeight(NewHeight)
		, TargetSlices(NewSlices)
	{}

	virtual FName GetNodeTypeName() const  { return "Enlarge v2"; }

	virtual void ComputeGraphHash(FHashBuilder& HashBuilder) const override
	{
		FTextureOperator::ComputeGraphHash(HashBuilder);
		HashBuilder << TargetWidth;
		HashBuilder << TargetHeight;
	}

	virtual FTextureDimension GetTextureDimension() const override
	{
		const FTextureDimension SourceDimension = SourceImage->GetTextureDimension();
		return {TargetWidth, TargetHeight, SourceDimension.Slices};
	}

	inline FIntVector TransformToSource(const FIntVector& Position) const
	{
		const FTextureDimension SourceDimension = SourceImage->GetTextureDimension();

		const FVector Ratio(
			(float)SourceDimension.Width / (float)TargetWidth,
			(float)SourceDimension.Height / (float)TargetHeight,
			(float)SourceDimension.Slices / (float)TargetSlices
		);

		return FIntVector(FVector(Position) * Ratio);
	}

	static inline float CalulateInterp(int32 TargetCoord, int32 TargetSize, int32 SourceSize)
	{
		return float((TargetCoord * SourceSize) % TargetSize) / float(TargetSize);
	}

	virtual void WriteChannel(int32 Channel, const FTextureDataTileDesc& Tile, float* TextureData) const override;

private:
	const int TargetWidth;
	const int TargetHeight;
	const int TargetSlices;
};
