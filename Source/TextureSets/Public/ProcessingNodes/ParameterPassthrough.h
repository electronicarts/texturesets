// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "IProcessingNode.h"

template <class ParameterClass>
class ParameterPassThrough : public IParameterProcessingNode
{
public:
	ParameterPassThrough(FName Name, int Dimension)
		: Name(Name)
		, Dimension(Dimension)
		, bInitialized(false)
	{}

	virtual FName GetNodeTypeName() const override { return Name; }

	virtual void Initialize(const FTextureSetProcessingContext& Context) override
	{
		check(!bInitialized);
		Value = GetValue(Context.GetAssetParam<ParameterClass>());
		bInitialized = true;
	}

	virtual bool IsInitialized() const override
	{
		return bInitialized;
	}

	virtual const uint32 ComputeGraphHash() const override
	{
		return GetTypeHash(GetNodeTypeName().ToString());
	}

	virtual const uint32 ComputeDataHash(const FTextureSetProcessingContext& Context) const override
	{
		return GetTypeHash(GetValue(Context.GetAssetParam<ParameterClass>()));
	}

	virtual FVector4f GetValue() const override { return Value; }

	virtual FVector4f GetValue(const ParameterClass* Parameter) const = 0;

private:
	FName Name;
	int Dimension;
	bool bInitialized;
	FVector4f Value;
};

#endif
