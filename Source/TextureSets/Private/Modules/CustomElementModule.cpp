// (c) Electronic Arts. All Rights Reserved.

#include "Modules/CustomElementModule.h"

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

void UCustomElementModule::BuildModuleInfo(FTextureSetDefinitionModuleInfo& Info) const
{
	FTextureSetSourceTextureDef TextureDef = FTextureSetSourceTextureDef{ ElementName, SRGB, ChannelCount, DefaultValue };
	Info.AddSourceTexture(TextureDef);
	Info.AddProcessedTexture(TextureDef);
}

void UCustomElementModule::BuildSamplingInfo(FTextureSetDefinitionSamplingInfo& SamplingInfo, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const EMaterialValueType ValueTypeLookup[4] = { MCT_Float1, MCT_Float2, MCT_Float3, MCT_Float4 };
	SamplingInfo.AddSampleOutput(ElementName, ValueTypeLookup[ChannelCount]);
}

void UCustomElementModule::Process(FTextureSetProcessingContext& Context) const
{
	// Just pass through source texture as processed texture
	Context.AddProcessedTexture(ElementName, Context.GetSourceTexture(ElementName));
}

int32 UCustomElementModule::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);

	Hash = HashCombine(Hash, GetTypeHash(ElementName));
	Hash = HashCombine(Hash, GetTypeHash(SRGB));
	Hash = HashCombine(Hash, GetTypeHash(ChannelCount));
	Hash = HashCombine(Hash, GetTypeHash(DefaultValue));

	return Hash;
}

#if WITH_EDITOR
void UCustomElementModule::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	// Simply connect texture sample to the matching output.
	TObjectPtr<UMaterialExpression> TextureExpression = Builder.GetProcessedTextureSample(ElementName);
	TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = Builder.GetOutput(ElementName);
	TextureExpression->ConnectExpression(OutputExpression->GetInput(0), 0);
}
#endif