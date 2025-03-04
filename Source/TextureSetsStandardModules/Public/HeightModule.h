// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "HeightModule.generated.h"

UCLASS()
class UHeightAssetParams : public UTextureSetAssetParams
{
	GENERATED_BODY()
public:

	// What is regarded as "neutral" height in the heightmap. A value of 0 means the heightmap is an extrusion, and a value of 1 is an inset.
	UPROPERTY(EditAnywhere, Category="Height", meta=(UIMin = "0.0", UIMax = "1.0"))
	float HeightmapReferencePlane = 1.0f;

	UPROPERTY(EditAnywhere, Category="Height", meta=(UIMin = "0.0", UIMax = "1.0", SliderExponent = "2"))
	float HeightmapScale = 0.05f;
};

UCLASS()
class UHeightSampleParams : public UTextureSetSampleParams
{
	GENERATED_BODY()
public:
	// Do parallax occlusion mapping, and affect other textures samples.
	UPROPERTY(EditAnywhere, Category="Height");
	bool bEnableParallaxOcclusionMapping = false;
};

// Adds a heightmap element to the texture set, and supports parallax occlusion mapping.
UCLASS()
class UHeightModule : public UTextureSetModule
{
	GENERATED_BODY()
public:
	virtual void GetAssetParamClasses(TSet<TSubclassOf<UTextureSetAssetParams>>& Classes) const override;
	virtual void GetSampleParamClasses(TSet<TSubclassOf<UTextureSetSampleParams>>& Classes) const override;
	
	virtual void ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual void ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
		FTextureSetMaterialGraphBuilder* Builder) const override;

	virtual EDataValidationResult IsDefinitionValid(const UTextureSetDefinition* Definition, FDataValidationContext& Context) const override;
};
