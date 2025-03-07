// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetSubsampleBuilder.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "TextureSetSampleFunctionBuilder.h"
#include "TextureSetModule.h"

FSubSampleAddress::FSubSampleAddress()
{
	UpdateHash();
}

FSubSampleAddress::FSubSampleAddress(const FSubSampleAddress& Base, SubSampleHandle Handle)
{
	HandleChain.Reserve(Base.HandleChain.Num() + 1);
	HandleChain = Base.HandleChain;
	HandleChain.Add(Handle);
	UpdateHash();
}

FSubSampleAddress::FSubSampleAddress(TConstArrayView<SubSampleHandle>& HandleChainView)
	: HandleChain(HandleChainView)
{
	UpdateHash();
}

const FSubSampleAddress& FSubSampleAddress::Root()
{
	static const FSubSampleAddress RootAddress;
	return RootAddress;
}

const FSubSampleAddress FSubSampleAddress::GetParent() const
{
	check(HandleChain.Num() > 0);

	if (HandleChain.Num() == 1)
	{
		return Root();
	}
	else
	{
		const SubSampleHandle* data = HandleChain.GetData();
		TConstArrayView<SubSampleHandle> HandleChainMinusOne(data, HandleChain.Num() - 1);
		return FSubSampleAddress(HandleChainMinusOne);
	}
}

void FSubSampleAddress::UpdateHash()
{
	Hash = 0;
	for (SubSampleHandle Handle : HandleChain)
	{
		Hash = HashCombine(Hash, GetTypeHash(Handle));
	}
}

void FTextureSetSubsampleBuilder::AddResult(FName Name, FGraphBuilderOutputPin Output)
{
	if (!ResultOwners.Contains(Name))
	{
		Results.Add(Name, Output);
		ResultOwners.Add(Name, Builder->GetWorkingModule());
	}
	else
	{
		Builder->LogError(FText::Format(INVTEXT("Sample result {0} has already been declared by {1}. Multiple results with the same name are not supported."),
			FText::FromName(Name),
			FText::FromString(ResultOwners.FindChecked(Name)->GetInstanceName())));
	}
}

const FGraphBuilderOutputPin& FTextureSetSubsampleBuilder::GetSharedValue(FName Name)
{
	return Builder->GetSharedValue(Address, Name);
}

void FTextureSetSubsampleBuilder::SetSharedValue(FName Name, FGraphBuilderOutputPin Pin)
{
	Builder->SetSharedValue(Address, Name, Pin);
}

const FGraphBuilderOutputPin& FTextureSetSubsampleBuilder::GetSharedValue(EGraphBuilderSharedValueType ValueType)
{
	return Builder->GetSharedValue(Address, ValueType);
}

void FTextureSetSubsampleBuilder::SetSharedValue(EGraphBuilderSharedValueType ValueType, FGraphBuilderOutputPin Pin)
{
	Builder->SetSharedValue(Address, ValueType, Pin);
}
