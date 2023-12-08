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

	virtual FName GetNodeTypeName() const  { return "Enlarge v2"; }

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
			(int)(Position.X * (float)SourceImage->GetWidth() / (float)Width),
			(int)(Position.Y * (float)SourceImage->GetHeight() / (float)Height),
			(int)(Position.Z * (float)SourceImage->GetSlices() / (float)Slices));
	}

	virtual void ComputeChunk(const FTextureProcessingChunk& Chunk, float* TextureData) const override
	{
		FTextureProcessingChunk SourceChunk = FTextureProcessingChunk(
			TransformToSource(Chunk.StartCoord),
			TransformToSource(Chunk.EndCoord),
			Chunk.Channel,
			0,
			1,
			SourceImage->GetWidth(),
			SourceImage->GetHeight(),
			SourceImage->GetSlices());

		TArray64<float> SourceTextureData;
		SourceTextureData.SetNumUninitialized(SourceChunk.DataEnd + 1);

		SourceImage->ComputeChunk(SourceChunk, SourceTextureData.GetData());

		int DataIndex = Chunk.DataStart;
		FIntVector Coord;
		FVector3f Lerp;
		for (Coord.Z = Chunk.StartCoord.Z; Coord.Z <= Chunk.EndCoord.Z; Coord.Z++)
		{
			Lerp.Z = FMath::Fmod(((float)Coord.Z / (float)Slices) * (float)SourceImage->GetSlices(), 1);

			for (Coord.Y = Chunk.StartCoord.Y; Coord.Y <= Chunk.EndCoord.Y; Coord.Y++)
			{
				Lerp.Y = FMath::Fmod(((float)Coord.Y / (float)Height) * (float)SourceImage->GetHeight(), 1);

				for (Coord.X = Chunk.StartCoord.X; Coord.X <= Chunk.EndCoord.X; Coord.X++)
				{
					Lerp.X = FMath::Fmod(((float)Coord.X / (float)Width) * (float)SourceImage->GetWidth(), 1);

					const FIntVector SourceBaseCoord = TransformToSource(Coord);

					float Values[2][2][2];

					for(int x = 0; x < 2; x++)
					{
						for(int y = 0; y < 2; y++)
						{
							for(int z = 0; z < 2; z++)
							{
								const FIntVector SourceCoord = SourceChunk.ClampCoord(SourceBaseCoord + FIntVector(x,y,z));
								Values[x][y][z] = SourceTextureData[SourceChunk.CoordToDataIndex(SourceCoord)];
							}
						}
					}

					// Lerp in Z
					if (Lerp.Z > 0)
					{
						for(int x = 0; x < 2; x++)
						{
							for(int y = 0; y < 2; y++)
							{
								Values[x][y][0] = FMath::Lerp(Values[x][y][0], Values[x][y][1], Lerp.Z);
							}
						}
					}

					// Lerp in Y
					if (Lerp.Y > 0)
					{
						for(int x = 0; x < 2; x++)
						{
							Values[x][0][0] = FMath::Lerp(Values[x][0][0], Values[x][1][0], Lerp.Y);
						}
					}

					// Lerp in X
					if (Lerp.X > 0)
					{
						Values[0][0][0] = FMath::Lerp(Values[0][0][0], Values[1][0][0], Lerp.X);
					}
					
					TextureData[DataIndex] = Values[0][0][0];
					DataIndex += Chunk.DataPixelStride;
				}
			}
		}
	}

private:
	const int Width;
	const int Height;
	const int Slices;
};
#endif
