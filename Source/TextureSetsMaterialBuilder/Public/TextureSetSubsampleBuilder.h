// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "GraphBuilderPin.h"
#include "GraphBuilderValue.h"

/*
* Subsamples are registered in sample groups in the graph builder.
Each sample group represents something that needs to be sampled multiple times and blended:
e.g. blended flipbook frame, triplanar projections, texture bombing, etc.
If there are multiple sample groups, they form a tree of samples that are blended together.

For example, imagine a flipbook (samples F0 and F1) that is also triplanar (X, Y, Z):
SampleGroups = {{F0, F1}, {X, Y, Z}}

It would form a tree of samples that looks something like:
    Root
   /    \
  F0    F1
/ | \  / | \
X Y Z  X Y Z

A SubSampleHandle is used to address a subsample definition within a group (e.g. F0, or Z)
A FSubSampleAddress is a chain of SubSampleHandles used to address a specific subsample in 
context (e.g. F0::X, or F1::Z)
*/

class FTextureSetSampleFunctionBuilder;
class FTextureSetSubsampleBuilder;

#define ConfigureSubsampleFunction TFunction<void(FTextureSetSubsampleBuilder&)>

typedef FGuid SubSampleHandle;

struct TEXTURESETSMATERIALBUILDER_API FSubSampleAddress
{
private:
	FSubSampleAddress();
public:
	FSubSampleAddress(const FSubSampleAddress& Base, SubSampleHandle Handle);
	FSubSampleAddress(TConstArrayView<SubSampleHandle>& HandleChain);

	static const FSubSampleAddress& Root();
	bool IsRoot() const	{ return GetHash() == Root().GetHash(); }
	uint32 GetHash() const { return Hash; }
	const TArray<SubSampleHandle>& GetHandleChain() const { return HandleChain; }

	const FSubSampleAddress GetParent() const;

	friend bool operator==(const FSubSampleAddress& A, const FSubSampleAddress& B) { return A.Hash == B.Hash; }
private:
	void UpdateHash();

	TArray<SubSampleHandle> HandleChain;
	uint32 Hash;
};

inline uint32 GetTypeHash(const FSubSampleAddress& Address)
{
	return Address.GetHash();
}

struct TEXTURESETSMATERIALBUILDER_API FSubSampleDefinition
{

public:
	FSubSampleDefinition(FName Name, FGraphBuilderOutputPin Weight)
		: Name(Name)
		, Weight(Weight)
	{}

	FSubSampleDefinition()
	{}

	FName Name;
	FGraphBuilderOutputPin Weight;
};

class TEXTURESETSMATERIALBUILDER_API FTextureSetSubsampleBuilder
{
	friend class FTextureSetSampleFunctionBuilder;

	FTextureSetSubsampleBuilder(FTextureSetSampleFunctionBuilder* Builder, FSubSampleAddress Address)
		: Builder(Builder)
		, Address(Address)
	{}

public:
	const FSubSampleAddress& GetAddress() { return Address; }

	bool IsRelevant(const SubSampleHandle& SubSample) { return Address.GetHandleChain().Contains(SubSample); }

	void AddResult(FName Name, FGraphBuilderOutputPin Output);

	// Valid for both texture names e.g. "BaseColor" and with channel suffix e.g. "BaseColor.g"
	const FGraphBuilderOutputPin& GetSharedValue(EGraphBuilderSharedValueType Value);
	const void SetSharedValue(FGraphBuilderOutputPin Address, EGraphBuilderSharedValueType Value);

	const FGraphBuilderOutputPin GetProcessedTextureSample(FName Name);

private:
	FTextureSetSampleFunctionBuilder* Builder;
	FSubSampleAddress Address;

	TMap<FName, FGraphBuilderValue> ProcessedTextureValues;

	TMap<FName, FGraphBuilderOutputPin> Results;
	TMap<FName, const class UTextureSetModule*> ResultOwners;
};
