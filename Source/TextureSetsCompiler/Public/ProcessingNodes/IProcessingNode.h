// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetProcessingContext.h"
#include "TextureSetInfo.h"
#include "TextureDataTileDesc.h"
#include "DerivedDataBuildVersion.h"

class FTextureSetProcessingGraph;

// Base class for processing nodes
class IProcessingNode
{
public:
	typedef UE::DerivedData::FBuildVersionBuilder FHashBuilder;

	virtual ~IProcessingNode() {}

	// Return a uniqe name for this node type
	virtual FName GetNodeTypeName() const = 0;

	// Computes the hash of the graph logic. Used to determine if the graph itself has changed.
	// Doesn't take into account any data, and may be called without initializing the node.
	// Result should recursively include hashes from dependent nodes' ComputeGraphHash() functions
	// Always executes on the game thread, so safe to access UObjects
	virtual void ComputeGraphHash(FHashBuilder& HashBuilder) const = 0;

	// Called immediately after the graph has been constructed, before any other functions of the node are used.
	// Used to read values from the context, and produce a hash to determine if a rebuild is required.
	// Should recursively include hashes from dependent nodes' ComputeDataHash() functions
	// Always executes on the game thread, so safe to access UObjects
	virtual void ComputeDataHash(const FTextureSetProcessingContext& Context, FHashBuilder& HashBuilder) const = 0;

	// Called when the compiler has determined a recomputation is required, directly before invoking any other functions.
	// All required data from the context should be copied into member variables, safe for multi-threaded access.
	// Should recursively invoke Prepare() on dependent nodes.
	// Always executes on the game thread, so safe to access UObjects.
	virtual void Prepare(const FTextureSetProcessingContext& Context) = 0;

	// Should recursively invoke Cache() on dependent nodes.
	// May execute on a worker thread, so not safe to access UObjects, and should be protected by a mutex.
	virtual void Cache() = 0;
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

	// Fetches the texture size. Called after the node has been prepared.
	virtual FTextureDimension GetTextureDimension() const = 0;

	// Gets the format of the texture output. Can be called BEFORE prepare()
	virtual const FTextureSetProcessedTextureDef GetTextureDef() const = 0;

	// Write a channel into the texture data.
	// May execute on a worker thread, so not safe to access UObjects
	virtual void WriteChannel(int32 Channel, const FTextureDataTileDesc& Tile, float* TextureData) const = 0;
};

// Processing node that computes a Vec4 parameter
class IParameterProcessingNode : public IProcessingNode
{
public:
	// Compute the parameter value
	virtual FVector4f GetValue() const = 0;
};
