// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "CustomElementModule.h"

#include "TextureSetProcessingGraph.h"
#include "TextureSetSampleFunctionBuilder.h"
#include "ProcessingNodes/TextureInput.h"

void UCustomElementModule::ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	TSharedRef<FTextureInput> Input = Graph.AddInputTexture(ElementName, ElementDef);

	Graph.AddOutputTexture(ElementName, Input);
}

void UCustomElementModule::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
	FTextureSetSampleFunctionBuilder* Builder) const
{
	Builder->AddSubsampleFunction(ConfigureSubsampleFunction([this, Builder](FTextureSetSubsampleBuilder& Subsample)
	{
		// Simply create a sample result for the element
		Subsample.AddResult(ElementName, Subsample.GetProcessedTextureSample(ElementName));
	}));
}
