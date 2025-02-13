// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetProcessingContext.h"
#include "TextureDataTileDesc.h"

class FTextureSetProcessingGraph;

// Base class for processing nodes
class IProcessingNode
{
public:
	virtual ~IProcessingNode() {}

	// Return a uniqe name for this node type
	virtual FName GetNodeTypeName() const = 0;

	// Loads resources required for executing the graph. Called before Initialize, and guaranteed to be on the game thread.
	// All required data from the context should be copied into member variables here to avoid race conditions.
	virtual void LoadResources(const FTextureSetProcessingContext& Context) = 0;

	// Initializes the graph, doing any pre-processing of data required.
	// This can be called on a worker thread, so should not load resources or modify UObjects
	virtual void Initialize(const FTextureSetProcessingGraph& Graph) = 0;

	// Computes the hash of the graph logic. Doesn't take into account any data, and can be called without initializing the node.
	// Result should include hashes from dependent nodes' ComputeGraphHash() functions
	virtual const uint32 ComputeGraphHash() const = 0;

	// Computes the hash of the graph's input data. Can be called without initializing the node.
	// Result should include hashes from dependent nodes' ComputeDataHash() functions
	virtual const uint32 ComputeDataHash(const FTextureSetProcessingContext& Context) const = 0;
};

// Processing node that computes texture data
class ITextureProcessingNode : public IProcessingNode
{
public:
	struct FTextureDimension
	{
		int Width;
		int Height;
		int Slices;
	};

	virtual FTextureDimension GetTextureDimension() const = 0;
	virtual const struct FTextureSetProcessedTextureDef GetTextureDef() const = 0;

	// Compute a texture channel and write into the texture data.
	virtual void ComputeChannel(int32 Channel, const FTextureDataTileDesc& Tile, float* TextureData) const = 0;
};

// Processing node that computes a Vec4 parameter
class IParameterProcessingNode : public IProcessingNode
{
public:
	// Compute the parameter value
	virtual FVector4f GetValue() const = 0;
};
