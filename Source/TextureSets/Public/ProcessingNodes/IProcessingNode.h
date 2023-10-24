// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "TextureSetProcessingContext.h"

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

class ITextureProcessingNode : public IProcessingNode
{
public:
	virtual int GetWidth() const = 0;
	virtual int GetHeight() const = 0;
	virtual int GetSlices() const = 0;
	virtual const struct FTextureSetProcessedTextureDef GetTextureDef() = 0;

	// Initialize must be called before calls to GetPixel
	virtual float GetPixel(int X, int Y, int Z, int Channel) const = 0;
};

class IParameterProcessingNode : public IProcessingNode
{
public:
	virtual FVector4f GetValue() const = 0;
};
#endif
