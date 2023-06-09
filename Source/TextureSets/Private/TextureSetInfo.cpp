// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetInfo.h"

void TextureSetDefinitionSharedInfo::AddSourceTexture(const TextureSetTextureDef& Texture)
{
	SourceTextures.Add(Texture);
}

void TextureSetDefinitionSharedInfo::AddProcessedTexture(const TextureSetTextureDef& Texture)
{
	checkf(!ProcessedTextureIndicies.Contains(Texture.Name), TEXT("Attempting to add processed texture %s twice"), Texture.Name);
	ProcessedTextureIndicies.Add(Texture.Name, ProcessedTextures.Num());
	ProcessedTextures.Add(Texture);
}

const TArray<TextureSetTextureDef> TextureSetDefinitionSharedInfo::GetSourceTextures() const
{
	return SourceTextures;
}

const TArray<TextureSetTextureDef> TextureSetDefinitionSharedInfo::GetProcessedTextures() const
{
	return ProcessedTextures;
}

const TextureSetTextureDef TextureSetDefinitionSharedInfo::GetProcessedTextureByName(FName Name) const
{
	return ProcessedTextures[ProcessedTextureIndicies.FindChecked(Name)];
}

FVector4 TextureSetPackingInfo::GetDefaultColor(int index) const
{
	return PackedTextureInfos[index].DefaultColor;
}

void TextureSetDefinitionSamplingInfo::AddMaterialParameter(FName Name, EMaterialValueType Type)
{
	checkf(!MaterialParameters.Contains(Name), TEXT("Attempting to add shader constant %s twice"), Name);
	MaterialParameters.Add(Name, Type);
}

void TextureSetDefinitionSamplingInfo::AddSampleInput(FName Name, EMaterialValueType Type)
{
	checkf(!SampleInputs.Contains(Name), TEXT("Attempting to add sample arguemnt %s twice"), Name);
	SampleInputs.Add(Name, Type);
}

void TextureSetDefinitionSamplingInfo::AddSampleOutput(FName Name, EMaterialValueType Type)
{
	checkf(!SampleOutputs.Contains(Name), TEXT("Attempting to add sample result %s twice"), Name);
	SampleOutputs.Add(Name, Type);
}

const TMap<FName, EMaterialValueType> TextureSetDefinitionSamplingInfo::GetMaterialParameters() const
{
	return MaterialParameters;
}

const TMap<FName, EMaterialValueType> TextureSetDefinitionSamplingInfo::GetSampleInputs() const
{
	return SampleInputs;
}

const TMap<FName, EMaterialValueType> TextureSetDefinitionSamplingInfo::GetSampleOutputs() const
{
	return SampleOutputs;
}
