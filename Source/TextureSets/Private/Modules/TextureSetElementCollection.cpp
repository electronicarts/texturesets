// (c) Electronic Arts. All Rights Reserved.

#include "Modules/TextureSetElementCollection.h"

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "ProcessingNodes/TextureInput.h"

#if WITH_EDITOR
void UTextureSetElementCollection::ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	// Just pass through source texture as processed texture
	for (const FElementDefinition& Element: Elements)
	{
		Graph.AddOutputTexture(Element.ElementName, Graph.AddInputTexture(Element.ElementName, Element.ElementDef));
	}
}
#endif

#if WITH_EDITOR
int32 UTextureSetElementCollection::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);
	for (const FElementDefinition& Element: Elements)
	{
		Hash = HashCombine(Hash, GetTypeHash(Element));
	}
	
	return Hash;
}
#endif

#if WITH_EDITOR
void UTextureSetElementCollection::ConfigureSamplingGraphBuilder(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder* Builder) const
{
	Builder->AddSampleBuilder(SampleBuilderFunction([this, Builder](FTextureSetSubsampleContext& SampleContext)
	{
		// Create a sample result for each texture
		for (const FElementDefinition& Element: Elements)
		{
			SampleContext.AddResult(Element.ElementName, SampleContext.GetProcessedTextureSample(Element.ElementName));
		}
	}));
}
#endif
