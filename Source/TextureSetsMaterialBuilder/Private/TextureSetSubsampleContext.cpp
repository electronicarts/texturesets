// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetSubsampleContext.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "TextureSetMaterialGraphBuilder.h"
#include "TextureSetModule.h"


#if WITH_EDITOR

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

FSubSampleAddress::FSubSampleAddress(const TArray<SubSampleHandle>& HandleChain)
	: HandleChain(HandleChain)
{
	UpdateHash();
}

const FSubSampleAddress& FSubSampleAddress::Root()
{
	static const FSubSampleAddress RootAddress;
	return RootAddress;
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

	FGraphBuilderValue& Value = SubsampleValues.FindOrAdd(ValueType);

	if (!Value.Reroute.IsValid())
	{
		// First time shared value was requested, create a reroute node for it and use that as the output
		UMaterialExpressionNamedRerouteDeclaration* Reroute = Builder->CreateReroute(UEnum::GetValueAsString(ValueType), Builder->SubsampleAddressToString(Address));
		Value.Reroute = FGraphBuilderOutputAddress(Reroute, 0);
	}

	return Value.Reroute;
}

const void FTextureSetSubsampleContext::SetSharedValue(FGraphBuilderOutputAddress OutputAddress, EGraphBuilderSharedValueType ValueType)
{
	FGraphBuilderValue& Value = SubsampleValues.FindOrAdd(ValueType);

	if (!Value.Source.IsValid())
	{
		Value.Source = OutputAddress;
		Value.Owner = Builder->GetWorkingModule();
	}
	else // Error logging
	{
		if (IsValid(Value.Owner))
		{
			Builder->LogError(FText::Format(INVTEXT("Shared value {0} has already been set by {1}. Multiple overrides are not currently supported"),
				UEnum::GetDisplayValueAsText(ValueType),
				FText::FromString(Value.Owner->GetInstanceName())));
		}
		else
		{
			Builder->LogError(FText::Format(INVTEXT("Shared value {0} has already been set. Multiple overrides are not currently supported"),
				UEnum::GetDisplayValueAsText(ValueType)));
		}
	}
}

const FGraphBuilderOutputAddress FTextureSetSubsampleContext::GetProcessedTextureSample(FName Name)
{
	FGraphBuilderValue& Value = ProcessedTextureValues.FindOrAdd(Name);

	if (!Value.Reroute.IsValid())
	{
		// First time texture sample value was requested, create a reroute node for it and use that as the output
		UMaterialExpressionNamedRerouteDeclaration* Reroute = Builder->CreateReroute(Name.ToString(), Builder->SubsampleAddressToString(Address));
		Value.Reroute = FGraphBuilderOutputAddress(Reroute, 0);
	}
	return Value.Reroute;
}

#endif // WITH_EDITOR