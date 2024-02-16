// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetElementCollection.h"

#include "TextureSetProcessingGraph.h"
#include "TextureSetMaterialGraphBuilder.h"
#include "ProcessingNodes/TextureInput.h"

void UTextureSetElementCollection::ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	// Just pass through source texture as processed texture
	for (const FElementDefinition& Element: Elements)
	{
		Graph.AddOutputTexture(Element.ElementName, Graph.AddInputTexture(Element.ElementName, Element.ElementDef));
	}
}

void UTextureSetElementCollection::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
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
