// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "CustomElementModule.generated.h"

// Allows users to define a custom input texture map that can be packed
// and unpacked with no extra processing.
UCLASS()
class UCustomElementModule : public UTextureSetModule
{
	GENERATED_BODY()
public:

	virtual FString GetInstanceName() const override { return ElementName.ToString(); }

	virtual void ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual void ConfigureSamplingGraphBuilder(
		const FTextureSetAssetParamsCollection* SampleParams,
		FTextureSetSampleFunctionBuilder* Builder) const override;

private:
	UPROPERTY(EditAnywhere, Category="CustomElement")
	FName ElementName;

	UPROPERTY(EditAnywhere, Category="CustomElement", meta=(ShowOnlyInnerProperties))
	FTextureSetSourceTextureDef ElementDef;

};