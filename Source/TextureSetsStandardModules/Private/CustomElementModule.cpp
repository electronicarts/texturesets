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
int32 UCustomElementModule::ComputeSamplingHash(const FTextureSetAssetParamsCollection* SampleParams) const
{
	uint32 Hash = Super::ComputeSamplingHash(SampleParams);

	Hash = HashCombine(Hash, GetTypeHash(ElementName.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(ElementDef));

	return Hash;
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
