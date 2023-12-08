// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "HeightModule.generated.h"

UCLASS()
class UHeightAssetParams : public UTextureSetAssetParams
{
	GENERATED_BODY()
public:

	// What is regarded as "neutral" height in the heightmap. A value of 0 means the heightmap is an extrusion, and a value of 1 is an inset.
	UPROPERTY(EditAnywhere, meta=(UIMin = "0.0", UIMax = "1.0"))
	float HeightmapReferencePlane = 1.0f;

	UPROPERTY(EditAnywhere, meta=(UIMin = "0.0", UIMax = "1.0", SliderExponent = "2"))
	float HeightmapScale = 0.05f;
};

UCLASS()
class UHeightSampleParams : public UTextureSetSampleParams
{
	GENERATED_BODY()
public:
	// Do parallax occlusion mapping, and affect other textures samples.
	UPROPERTY(EditAnywhere);
	bool bEnableParallaxOcclusionMapping = false;
};

UCLASS()
class UHeightModule : public UTextureSetModule
{
	GENERATED_BODY()
public:
	virtual bool AllowMultiple() const override { return false; }
	virtual TSubclassOf<UTextureSetAssetParams> GetAssetParamClass() const override { return UHeightAssetParams::StaticClass(); }
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const override { return UHeightSampleParams::StaticClass(); }
	
	virtual void ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;

	virtual void ConfigureSamplingGraphBuilder(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
		FTextureSetMaterialGraphBuilder* Builder) const override;

	virtual EDataValidationResult IsDefinitionValid(const UTextureSetDefinition* Definition, FDataValidationContext& Context) const override;
};