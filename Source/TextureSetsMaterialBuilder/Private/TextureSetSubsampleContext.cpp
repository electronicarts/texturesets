// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetSubsampleContext.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "TextureSetMaterialGraphBuilder.h"
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

void FTextureSetSubsampleContext::AddResult(FName Name, FGraphBuilderOutputAddress Output)
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

const FGraphBuilderOutputAddress& FTextureSetSubsampleContext::GetSharedValue(EGraphBuilderSharedValueType ValueType)
{
	return Builder->GetSharedValue(Address, ValueType);
}

const void FTextureSetSubsampleContext::SetSharedValue(FGraphBuilderOutputAddress OutputAddress, EGraphBuilderSharedValueType ValueType)
{
	Builder->SetSharedValue(Address, OutputAddress, ValueType);
}

const FGraphBuilderOutputAddress FTextureSetSubsampleContext::GetProcessedTextureSample(FName Name)
{
	FGraphBuilderValue& Value = ProcessedTextureValues.FindOrAdd(Name);

	if (!Value.Reroute.IsValid())
	{
		// First time texture sample value was requested, create a reroute node for it and use that as the output
		UMaterialExpressionNamedRerouteDeclaration* Reroute = Builder->CreateReroute(Name.ToString(), Address);
		Value.Reroute = FGraphBuilderOutputAddress(Reroute, 0);
	}
	return Value.Reroute;
}
