// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "IProcessingNode.h"

template <class ParameterClass>
class ParameterPassThrough : public IParameterProcessingNode
{
public:
	ParameterPassThrough(FName Name, TFunction<FVector4f(const ParameterClass*)> GetValueCallback)
		: Name(Name)
		, bInitialized(false)
		, Callback(GetValueCallback)
	{}

	virtual FName GetNodeTypeName() const override { return Name; }

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override
	{
		// No resources to load
	}

	virtual void Initialize(const FTextureSetProcessingContext& Context) override
	{
		check(!bInitialized);
		Value = Callback(Context.GetAssetParam<ParameterClass>());
		bInitialized = true;
	}

	virtual const uint32 ComputeGraphHash() const override
	{
		return GetTypeHash(GetNodeTypeName().ToString());
	}

	virtual const uint32 ComputeDataHash(const FTextureSetProcessingContext& Context) const override
	{
		return GetTypeHash(Callback(Context.GetAssetParam<ParameterClass>()));
	}

	virtual FVector4f GetValue() const override { return Value; }

private:
	FName Name;
	bool bInitialized;
	TFunction<FVector4f(const ParameterClass*)> Callback;
	FVector4f Value;
};

#endif
