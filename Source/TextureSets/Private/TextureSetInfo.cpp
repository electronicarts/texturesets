// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetInfo.h"

void FTextureSetDefinitionSamplingInfo::AddMaterialParameter(FName Name, EMaterialValueType Type)
{
	checkf(!MaterialParameters.Contains(Name), TEXT("Attempting to add shader constant %s twice"), Name);
	MaterialParameters.Add(Name, Type);
}

void FTextureSetDefinitionSamplingInfo::AddSampleInput(FName Name, EMaterialValueType Type)
{
	checkf(!SampleInputs.Contains(Name), TEXT("Attempting to add sample arguemnt %s twice"), Name);
	SampleInputs.Add(Name, Type);
}

void FTextureSetDefinitionSamplingInfo::AddSampleOutput(FName Name, EMaterialValueType Type)
{
	checkf(!SampleOutputs.Contains(Name), TEXT("Attempting to add sample result %s twice"), Name);
	SampleOutputs.Add(Name, Type);
}

const TMap<FName, EMaterialValueType> FTextureSetDefinitionSamplingInfo::GetMaterialParameters() const
{
	return MaterialParameters;
}

const TMap<FName, EMaterialValueType> FTextureSetDefinitionSamplingInfo::GetSampleInputs() const
{
	return SampleInputs;
}

const TMap<FName, EMaterialValueType> FTextureSetDefinitionSamplingInfo::GetSampleOutputs() const
{
	return SampleOutputs;
}