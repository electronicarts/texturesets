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

void UCustomElementModule::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
	FTextureSetMaterialGraphBuilder* Builder) const
{
	Builder->AddSampleBuilder(SampleBuilderFunction([this, Builder](FTextureSetSubsampleContext& SampleContext)
	{
		// Simply create a sample result for the element
		SampleContext.AddResult(ElementName, SampleContext.GetProcessedTextureSample(ElementName));
	}));
}
