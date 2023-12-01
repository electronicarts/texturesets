// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "TextureSetProcessingContext.h"

#define PROCESSING_METHOD_PERPIXEL 1
#define PROCESSING_METHOD_CHUNK 2

#define PROCESSING_METHOD PROCESSING_METHOD_CHUNK

class FTextureSetProcessingGraph;

class IProcessingNode
{
public:
	virtual ~IProcessingNode() {}

	// Return a uniqe name for this node type
	virtual FName GetNodeTypeName() const = 0;

	// Loads resources required for execuring the graph. Called before Initialize, and guaranteed to be on the game thread.
	virtual void LoadResources(const FTextureSetProcessingContext& Context) = 0;

	// Initializes the graph, doing any pre-processing of data required.
	// This can be called on a worker thread, so should not load resources or modify UObjects
	virtual void Initialize(const FTextureSetProcessingGraph& Graph) = 0;

	// Computes the hash of the graph logic. Doesn't take into account any data, and can be called without initializing the node.
	virtual const uint32 ComputeGraphHash() const = 0;

	// Computes the hash of the graph's input data. Can be called without initializing the node.
	virtual const uint32 ComputeDataHash(const FTextureSetProcessingContext& Context) const = 0;
};

#if PROCESSING_METHOD == PROCESSING_METHOD_CHUNK
struct FTextureProcessingChunk
{
public:
	FTextureProcessingChunk(FIntVector StartCoord, FIntVector EndCoord, int Channel, int DataStart, int DataPixelStride, int TextureWidth, int TextureHeight, int TextureSlices)
		: StartCoord(StartCoord)
		, EndCoord(EndCoord)
		, Channel(Channel)
		, DataStart(DataStart)
		, DataPixelStride(DataPixelStride)
		, TextureWidth(TextureWidth)
		, TextureHeight(TextureHeight)
		, TextureSlices(TextureSlices)
	{
		FirstPixel = CoordToPixel(StartCoord);
		LastPixel = CoordToPixel(EndCoord);
		check(FirstPixel <= LastPixel)

		NumPixels = LastPixel - FirstPixel + 1;
		DataEnd = DataStart + ((LastPixel - FirstPixel) * DataPixelStride);
	}

	// Passed in
	FIntVector StartCoord; // Coordinate of the first pixel we're processing
	FIntVector EndCoord; // Coordinate of the last pixel we're processing
	int Channel; // Processed texture channel that we're computing, indexed from RGBA
	int DataStart; // First index into the data buffer where we are expected to write
	int DataPixelStride; // Stride between pixels
	int TextureWidth; // How many pixels wide is the entire texture we're processing
	int TextureHeight; // How many pixels high is the entire texture we're processing
	int TextureSlices; // How many slices in the entire texture we're processing

	// Computed
	int FirstPixel; // Index of the first pixel we are processing in the chunk
	int LastPixel; // Index of the last pixel we are processing
	int NumPixels; // How many pixels are we processing
	int DataEnd; // Last index into the data buffer where we are expected to write

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
		return DataStart + ((Pixel - FirstPixel) * DataPixelStride);
	}

	inline int NextDataIndex(const int& DataIndex) const
	{
		return DataIndex + DataPixelStride;
	}
};
#endif

class ITextureProcessingNode : public IProcessingNode
{
public:
	virtual int GetWidth() const = 0;
	virtual int GetHeight() const = 0;
	virtual int GetSlices() const = 0;
	virtual const struct FTextureSetProcessedTextureDef GetTextureDef() = 0;

#if PROCESSING_METHOD == PROCESSING_METHOD_CHUNK
	virtual void ComputeChunk(const FTextureProcessingChunk& Chunk, float* TextureData) const = 0;
#else
	// Initialize must be called before calls to GetPixel
	virtual float GetPixel(int X, int Y, int Z, int Channel) const = 0;
#endif
};

class IParameterProcessingNode : public IProcessingNode
{
public:
	virtual FVector4f GetValue() const = 0;
};
#endif
