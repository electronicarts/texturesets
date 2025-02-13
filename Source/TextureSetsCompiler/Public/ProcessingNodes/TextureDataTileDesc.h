// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FTextureDataTileDesc
{
public:
	FTextureDataTileDesc(FIntVector3 TextureSize, FIntVector3 TileSize, FIntVector3 TileOffset, FIntVector3 TileDataStride, int32 TileDataOffset)
		: TextureSize(TextureSize)
		, TileSize(TileSize)
		, TileOffset(TileOffset)
		, TileDataOffset(TileDataOffset)
		, TileDataStride(TileDataStride)
	{
		// Ensure our tile lies within the texture
		check(TileOffset.X >= 0);
		check(TileOffset.Y >= 0);
		check(TileOffset.Z >= 0);
		check(TileOffset.X + TileSize.X <= TextureSize.X);
		check(TileOffset.Y + TileSize.Y <= TextureSize.X);
		check(TileOffset.Z + TileSize.Z <= TextureSize.X);

		// Check if we might get out of range because our texture is too large
		check(((int64)TextureSize.X * (int64)TextureSize.Y * (int64)TextureSize.Z) < (int64)MAX_int32);
		check(((int64)TileDataStride.Z * (int64)TextureSize.Z + 1) < (int64)MAX_int32);

		TileDataStepSize.X = TileDataStride.X;
		TileDataStepSize.Y = TileDataStride.Y - TileDataStepSize.X * TileSize.X;
		TileDataStepSize.Z = TileDataStride.Z - TileDataStride.Y * TileSize.Y - TileDataStepSize.Y;
	}

	// Passed in
	const FIntVector3 TextureSize; // Size of the entire texture, in pixels
	const FIntVector3 TileSize; // Size of the tile, in pixels
	const FIntVector3 TileOffset; // Offset of the tile within the texture, in pixels
	const int32 TileDataOffset; // Index into the data where this tile starts
	const FIntVector3 TileDataStride; // Stride in each direction of the tile data

	// Computed
	FIntVector3 TileDataStepSize; // Step size when iterating through the tile data in each dimension
	
	// Helper function to compute the data srides based on element stride, and the dimensions of the full image buffer's data
	inline static FIntVector3 ComputeDataStrides(int32 ElementStride, FIntVector3 DataPixelDimension)
	{
		FIntVector3 DataStrides;
		DataStrides.X = ElementStride;
		DataStrides.Y = DataPixelDimension.X * DataStrides.X;
		DataStrides.Z = DataPixelDimension.Y * DataStrides.Y;
		return DataStrides;
	}

	// Helper function to compute data offset.
	inline static int64 ComputeDataOffset(FIntVector3 TileOffset, FIntVector3 DataStrides)
	{
		return (TileOffset.X * DataStrides.X) + (TileOffset.Y * DataStrides.Y) + (TileOffset.Z * DataStrides.Z);
	}

	// Takes a tile coordinate and produces an index into the tile data
	inline uint32 TileCoordToDataIndex(const FIntVector& Coord) const
	{
		return TileDataOffset + (Coord.X * TileDataStride.X) + (Coord.Y * TileDataStride.Y) + (Coord.Z * TileDataStride.Z);
	}

	struct ForEachPixelContext
	{
		FIntVector3 TileCoord;
		int64 DataIndex;
	};

	// Helper function for quick iteration over all data of this tile
	// See TextureSetCompiler.cpp for usage examples
	template <typename Lambda>
	void ForEachPixel(const Lambda& Func) const
	{
		ForEachPixelContext Context;
		Context.DataIndex = TileDataOffset;

		for (Context.TileCoord.Z = 0; Context.TileCoord.Z < TileSize.Z; Context.TileCoord.Z++)
		{
			for (Context.TileCoord.Y = 0; Context.TileCoord.Y < TileSize.Y; Context.TileCoord.Y++)
			{
				for (Context.TileCoord.X = 0; Context.TileCoord.X < TileSize.X; Context.TileCoord.X++)
				{
					Func(Context);
					Context.DataIndex += TileDataStepSize.X; // Next Pixel
				}
				Context.DataIndex += TileDataStepSize.Y; // Next row
			}
			Context.DataIndex += TileDataStepSize.Z; // Next slice
		}
	}
};
