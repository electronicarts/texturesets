// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FTextureChannelDataDesc
{
public:
	FTextureChannelDataDesc(int ChannelIndex, int DataStart, int DataPixelStride, int TextureWidth, int TextureHeight, int TextureSlices)
		: ChannelIndex(ChannelIndex)
		, DataStart(DataStart)
		, DataPixelStride(DataPixelStride)
		, TextureWidth(TextureWidth)
		, TextureHeight(TextureHeight)
		, TextureSlices(TextureSlices)
		, NumPixels(TextureWidth * TextureHeight * TextureSlices)
		, DataEnd(DataStart + ((NumPixels - 1) * DataPixelStride))
	{
	}

	// Passed in
	const int ChannelIndex; // Processed texture channel that we're computing, indexed from RGBA
	const int DataStart; // First index into the data buffer where we are expected to write
	const int DataPixelStride; // Stride between pixels
	const int TextureWidth; // How many pixels wide is the entire texture we're processing
	const int TextureHeight; // How many pixels high is the entire texture we're processing
	const int TextureSlices; // How many slices in the entire texture we're processing

	// Computed
	const int NumPixels; // How many pixels are we processing
	const int DataEnd; // Last index into the data buffer where we are expected to write

	// Convert a coordinate (XYZ) into a pixel index
	inline int CoordToPixel(const FIntVector& Coord) const
	{
		return (Coord.Z * TextureWidth * TextureHeight) + (Coord.Y * TextureWidth) + Coord.X;
	}

	// Clamps the coordinate to be within the texture's size
	inline const FIntVector ClampCoord(const FIntVector& Coord) const
	{
		return FIntVector(FMath::Clamp(Coord.X, 0, TextureWidth - 1), FMath::Clamp(Coord.Y, 0, TextureHeight - 1), FMath::Clamp(Coord.Z, 0, TextureSlices - 1));
	}

	// Convert a coordinate (XYZ) into a data index (includes stride and data offset)
	// Can then be used to lookup into the data buffer
	inline int CoordToDataIndex(const FIntVector& Coord) const
	{
		return PixelToDataIndex(CoordToPixel(Coord));
	}

	// Convert a pixel into a data index (includes stride and data offset)
	// Can then be used to lookup into the data buffer
	inline int PixelToDataIndex(const int& Pixel) const
	{
		return DataStart + (Pixel * DataPixelStride);
	}

	inline int NextDataIndex(const int& DataIndex) const
	{
		return DataIndex + DataPixelStride;
	}

	template <typename Lambda>
	void ForEachPixel(const Lambda& Func) const
	{
		for (int64 DataIndex = DataStart; DataIndex <= DataEnd; DataIndex += DataPixelStride)
		{
			Func(DataIndex);
		}
	}

	template <typename Lambda>
	void ForEachPixelParallel(int PixelsPerJob, const Lambda& Func) const
	{
		check(PixelsPerJob > 0);
		const int NumJobs = 1 + ((PixelsPerJob - 1) / NumPixels);

		ParallelFor(NumJobs, [*this, PixelsPerJob, &Func](int64 JobIndex)
		{
			const int64 StartPixel = JobIndex * PixelsPerJob;
			const int64 EndPixel = FMath::Min(StartPixel + PixelsPerJob, NumPixels);
	
			const int64 JobDataStart = DataStart + (StartPixel * DataPixelStride);
			const int64 JobDataEnd = DataStart + (EndPixel * DataPixelStride);
	
			for (int64 DataIndex = JobDataStart; DataIndex <= JobDataEnd; DataIndex += DataPixelStride)
			{
				Func(DataIndex);
			}
		});
	}

	template <typename Lambda>
	void ForEachPixelParallel(const Lambda& Func) const
	{
		ForEachPixelParallel(256 * 256, Func);
	}
};