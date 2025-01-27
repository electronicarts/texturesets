// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "CustomElementModule.h"

#include "TextureSetProcessingGraph.h"
#include "TextureSetMaterialGraphBuilder.h"
#include "ProcessingNodes/TextureInput.h"

void UCustomElementModule::ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	TSharedRef<FTextureInput> Input = Graph.AddInputTexture(ElementName, ElementDef);

	Graph.AddOutputTexture(ElementName, Input);
}

int32 UCustomElementModule::ComputeSamplingHash(const FTextureSetAssetParamsCollection* SampleParams) const
{
	uint32 Hash = Super::ComputeSamplingHash(SampleParams);

	Hash = HashCombine(Hash, GetTypeHash(ElementName.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(ElementDef));

	return Hash;
}

void UCustomElementModule::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
	FTextureSetMaterialGraphBuilder* Builder) const
{
	Builder->AddSampleBuilder(SampleBuilderFunction([this, Builder](FTextureSetSubsampleContext& SampleContext)
	{
		// Simply create a sample result for the element
		SampleContext.AddResult(ElementName, SampleContext.GetProcessedTextureSample(ElementName));
	}));
}
