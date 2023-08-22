// (c) Electronic Arts. All Rights Reserved.

#include "Modules/TextureSetElementCollection.h"

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

void UTextureSetElementCollection::GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	// Just pass through source texture as processed texture
	for (const FElementDefinition& Element: Elements)
	{
		FTextureSetSourceTextureDef TextureDef;
		TextureDef.SRGB = Element.SRGB;
		TextureDef.ChannelCount = Element.ChannelCount;
		TextureDef.DefaultValue = Element.DefaultValue;

		Graph.AddOutputTexture(Element.ElementName, Graph.AddInputTexture(Element.ElementName, TextureDef));
	}
}

int32 UTextureSetElementCollection::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);
	for (const FElementDefinition& Element: Elements)
	{
		Hash = HashCombine(Hash, GetTypeHash(Element));
	}
	
	return Hash;
}

#if WITH_EDITOR
void UTextureSetElementCollection::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	for (const FElementDefinition& Element: Elements)
	{
		// Simply connect texture sample to the matching output.
		TObjectPtr<UMaterialExpression> TextureExpression = Builder.GetProcessedTextureSample(Element.ElementName);
		TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = Builder.CreateOutput(Element.ElementName);
		TextureExpression->ConnectExpression(OutputExpression->GetInput(0), 0);
	}
}
#endif
