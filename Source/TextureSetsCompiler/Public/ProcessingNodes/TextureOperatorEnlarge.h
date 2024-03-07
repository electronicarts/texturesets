// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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

	virtual const uint32 ComputeGraphHash() const override
	{
		uint32 Hash = FTextureOperator::ComputeGraphHash();

		Hash = HashCombine(Hash, GetTypeHash(TargetWidth));
		Hash = HashCombine(Hash, GetTypeHash(TargetHeight));

		return Hash;
	}

	virtual FTextureDimension GetTextureDimension() const override
	{
		const FTextureDimension SourceDimension = SourceImage->GetTextureDimension();
		return {TargetWidth, TargetHeight, SourceDimension.Slices};
	}

	inline FIntVector TransformToSource(const FIntVector& Position) const
	{
		const FTextureDimension SourceDimension = SourceImage->GetTextureDimension();

		return FIntVector(
			(int)(Position.X * (float)SourceDimension.Width / (float)TargetWidth),
			(int)(Position.Y * (float)SourceDimension.Height / (float)TargetHeight),
			(int)(Position.Z * (float)SourceDimension.Slices / (float)TargetSlices));
	}

	virtual void ComputeChunk(const FTextureProcessingChunk& Chunk, float* TextureData) const override
	{
		const FTextureDimension SourceDimension = SourceImage->GetTextureDimension();

		FTextureProcessingChunk SourceChunk = FTextureProcessingChunk(
			Chunk.Channel,
			0,
			1,
			SourceDimension.Width,
			SourceDimension.Height,
			SourceDimension.Slices);

		TArray64<float> SourceTextureData;
		SourceTextureData.SetNumUninitialized(SourceChunk.DataEnd + 1);

		SourceImage->ComputeChunk(SourceChunk, SourceTextureData.GetData());

		int DataIndex = Chunk.DataStart;
		FIntVector Coord;
		FVector3f Lerp;
		for (Coord.Z = 0; Coord.Z < Chunk.TextureSlices; Coord.Z++)
		{
			Lerp.Z = FMath::Fmod(((float)Coord.Z / (float)TargetSlices) * (float)SourceDimension.Slices, 1);

			for (Coord.Y = 0; Coord.Y < Chunk.TextureHeight; Coord.Y++)
			{
				Lerp.Y = FMath::Fmod(((float)Coord.Y / (float)TargetHeight) * (float)SourceDimension.Height, 1);

				for (Coord.X = 0; Coord.X < Chunk.TextureWidth; Coord.X++)
				{
					Lerp.X = FMath::Fmod(((float)Coord.X / (float)TargetWidth) * (float)SourceDimension.Width, 1);

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
	const int TargetWidth;
	const int TargetHeight;
	const int TargetSlices;
};
