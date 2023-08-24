// (c) Electronic Arts. All Rights Reserved.

#include "Modules/CustomElementModule.h"

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

#if WITH_EDITOR
void UCustomElementModule::GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	// Just pass through source texture as processed texture
	FTextureSetSourceTextureDef TextureDef = FTextureSetSourceTextureDef{ SRGB, ChannelCount, DefaultValue };

	Graph.AddOutputTexture(ElementName, Graph.AddInputTexture(ElementName, TextureDef));
}
#endif

#if WITH_EDITOR
int32 UCustomElementModule::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);

	Hash = HashCombine(Hash, GetTypeHash(ElementName.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(SRGB));
	Hash = HashCombine(Hash, GetTypeHash(ChannelCount));
	Hash = HashCombine(Hash, GetTypeHash(DefaultValue));

	return Hash;
}
#endif

#if WITH_EDITOR
void UCustomElementModule::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	// Simply connect texture sample to the matching output.
	TObjectPtr<UMaterialExpression> TextureExpression = Builder.GetProcessedTextureSample(ElementName);
	TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = Builder.CreateOutput(ElementName);
	TextureExpression->ConnectExpression(OutputExpression->GetInput(0), 0);
}
#endif
