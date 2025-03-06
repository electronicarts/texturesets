// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "ElementCollection/TextureSetElementCollection.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "ElementCollection/TextureSetElementCollectionFactory.h"
#include "Engine/AssetManager.h"
#include "ProcessingNodes/TextureInput.h"
#include "TextureSetDefinition.h"
#include "TextureSetSampleFunctionBuilder.h"
#include "TextureSetProcessingGraph.h"

FString UTextureSetElementCollectionModule::GetInstanceName() const {
  if (IsValid(Collection))
    return Collection->GetName();
  else
    return Super::GetInstanceName();
}

void UTextureSetElementCollectionModule::ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	if (IsValid(Collection))
	{
		// Just pass through source texture as processed texture
		for (const auto& [ElementName, ElementDef]: Collection->Elements)
		{
			Graph.AddOutputTexture(ElementName, Graph.AddInputTexture(ElementName, ElementDef));
		}
	}
}

void UTextureSetElementCollectionModule::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
	FTextureSetSampleFunctionBuilder* Builder) const
{
	if (IsValid(Collection))
	{
		Builder->AddSubsampleFunction(ConfigureSubsampleFunction([this, Builder](FTextureSetSubsampleBuilder& Subsample)
		{
			// Create a sample result for each texture
			for (const auto& [ElementName, ElementDef]: Collection->Elements)
			{
				Subsample.AddResult(ElementName, Subsample.GetProcessedTextureSample(ElementName));
			}
		}));
	}
}
