// (c) Electronic Arts. All Rights Reserved.

#include "CustomElementModule.h"

#include "TextureSetProcessingGraph.h"
#include "TextureSetMaterialGraphBuilder.h"
#include "ProcessingNodes/TextureInput.h"

#if WITH_EDITOR
void UCustomElementModule::ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	Graph.AddOutputTexture(ElementName, Graph.AddInputTexture(ElementName, ElementDef));
}
#endif

#if WITH_EDITOR
void UCustomElementModule::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
	FTextureSetMaterialGraphBuilder* Builder) const
{
	Builder->AddSampleBuilder(SampleBuilderFunction([this, Builder](FTextureSetSubsampleContext& SampleContext)
	{
		// Simply create a sample result for the element
		SampleContext.AddResult(ElementName, SampleContext.GetProcessedTextureSample(ElementName));
	}));
}
#endif
