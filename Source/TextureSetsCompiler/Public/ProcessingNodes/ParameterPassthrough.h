// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "IProcessingNode.h"

template <class ParameterClass>
class ParameterPassThrough : public IParameterProcessingNode
{
public:
	ParameterPassThrough(FName Name, TFunction<FVector4f(const ParameterClass*)> GetValueCallback)
		: Name(Name)
		, Callback(GetValueCallback)
	{}

	virtual FName GetNodeTypeName() const override { return Name; }

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override
	{
		Value = Callback(Context.AssetParams.Get<ParameterClass>());
	}

	virtual void Initialize(const FTextureSetProcessingGraph& Graph) override
	{
		// No initialization needed
	}

	virtual void ComputeGraphHash(FHashBuilder& HashBuilder) const override
	{
		HashBuilder << GetNodeTypeName();
	}

	virtual void ComputeDataHash(const FTextureSetProcessingContext& Context, FHashBuilder& HashBuilder) const override
	{
		HashBuilder << Callback(Context.AssetParams.Get<ParameterClass>());
	}

	virtual FVector4f GetValue() const override { return Value; }

private:
	FName Name;
	TFunction<FVector4f(const ParameterClass*)> Callback;
	FVector4f Value;
};
