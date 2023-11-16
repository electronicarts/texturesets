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

	inline FIntVector TransformToSource(const FIntVector& Position) const
	{
		return FIntVector(
			Position.X * (SourceImage->GetWidth() / Width),
			Position.Y * (SourceImage->GetHeight() / Height),
			Position.Z * (SourceImage->GetSlices() / Slices)
		);
	}

#if PROCESSING_METHOD == PROCESSING_METHOD_CHUNK
	virtual void ComputeChunk(const FTextureProcessingChunk& Chunk, float* TextureData) const override
	{
		FTextureProcessingChunk SourceChunk = FTextureProcessingChunk(
			TransformToSource(Chunk.StartCoord),
			TransformToSource(Chunk.EndCoord),
			Chunk.Channel,
			0,
			1,
			Width,
			Height);

		TArray64<float> SourceTextureData;
		SourceTextureData.SetNumUninitialized(SourceChunk.DataEnd + 1);

		SourceImage->ComputeChunk(SourceChunk, SourceTextureData.GetData());

		int DataIndex = Chunk.DataStart;
		FIntVector Coord;
		for (Coord.Z = Chunk.StartCoord.Z; Coord.Z <= Chunk.EndCoord.Z; Coord.Z++)
		{
			for (Coord.Y = Chunk.StartCoord.Y; Coord.Y <= Chunk.EndCoord.Y; Coord.Y++)
			{
				for (Coord.X = Chunk.StartCoord.X; Coord.X <= Chunk.EndCoord.X; Coord.X++)
				{
					// TODO: Bilinear/Trilinear interpolation
					const int SourceDataIndex = SourceChunk.CoordToDataIndex(TransformToSource(Coord));
					TextureData[DataIndex] = SourceTextureData[SourceDataIndex];
					DataIndex += Chunk.DataPixelStride;
				}
			}
		}
	}
#else
	virtual float GetPixel(int X, int Y, int Z, int Channel) const override
	{
		// TODO: Bilinear/Trilinear interpolation
		const int SourceX = X * (SourceImage->GetWidth() / Width);
		const int SourceY = Y * (SourceImage->GetHeight() / Height);
		const int SourceZ = Z * (SourceImage->GetSlices() / Slices);

		return SourceImage->GetPixel(SourceX, SourceY, SourceZ, Channel);
	}
#endif

private:
	const int Width;
	const int Height;
	const int Slices;
};
#endif
