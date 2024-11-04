// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "CustomElementModule.generated.h"

// Allows creation of custom texture map elements, with explicit control over channel count, default values, and encoding.
UCLASS()
class UCustomElementModule : public UTextureSetModule
{
	GENERATED_BODY()
public:

	virtual FString GetInstanceName() const override { return ElementName.ToString(); }

	virtual void ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual int32 ComputeSamplingHash(const FTextureSetAssetParamsCollection* SampleParams) const override;

	virtual void ConfigureSamplingGraphBuilder(
		const FTextureSetAssetParamsCollection* SampleParams,
		FTextureSetMaterialGraphBuilder* Builder) const override;

private:
	UPROPERTY(EditAnywhere, Category="CustomElement")
	FName ElementName;

	UPROPERTY(EditAnywhere, Category="CustomElement", meta=(ShowOnlyInnerProperties))
	FTextureSetSourceTextureDef ElementDef;

};