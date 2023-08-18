// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetInfo.h"

void FTextureSetDefinitionModuleInfo::AddSourceTexture(const FTextureSetSourceTextureDef& Texture)
{
	SourceTextures.Add(Texture);
}

void FTextureSetDefinitionModuleInfo::AddProcessedTexture(const FTextureSetProcessedTextureDef& Texture)
{
	checkf(!ProcessedTextureIndicies.Contains(Texture.Name), TEXT("Attempting to add processed texture %s twice"), Texture.Name);
	ProcessedTextureIndicies.Add(Texture.Name, ProcessedTextures.Num());
	ProcessedTextures.Add(Texture);
}

const TArray<FTextureSetSourceTextureDef> FTextureSetDefinitionModuleInfo::GetSourceTextures() const
{
	return SourceTextures;
}

const TArray<FTextureSetProcessedTextureDef> FTextureSetDefinitionModuleInfo::GetProcessedTextures() const
{
	return ProcessedTextures;
}

const FTextureSetProcessedTextureDef FTextureSetDefinitionModuleInfo::GetProcessedTextureByName(FName Name) const
{
	return ProcessedTextures[ProcessedTextureIndicies.FindChecked(Name)];
}

const bool FTextureSetDefinitionModuleInfo::HasProcessedTextureOfName(FName Name) const
{
	return ProcessedTextureIndicies.Contains(Name);
}

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