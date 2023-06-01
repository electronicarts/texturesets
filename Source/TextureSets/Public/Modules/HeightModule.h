// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetDefinition.h"
#include "HeightModule.generated.h"

UCLASS()
class UHeightAssetParams : public UTextureSetAssetParams
{
	GENERATED_BODY()
public:
	// What is regarded as "neutral" height in the heightmap. A value of 0 means the heightmap is an extrusion, and a value of 1 is an inset.
	UPROPERTY(EditAnywhere, meta=(UIMin = "0.0", UIMax = "1.0"))
	float HeightmapReferencePlane = 1.0f;
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
class UHeightModule : public UTextureSetDefinitionModule
{
	GENERATED_BODY()
public:
	virtual bool AllowMultiple() override { return false; }
	virtual TSubclassOf<UTextureSetAssetParams> GetAssetParamClass() const { return UHeightAssetParams::StaticClass(); }
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const override { return UHeightSampleParams::StaticClass(); }
	
	virtual void BuildSharedInfo(TextureSetDefinitionSharedInfo& Info);

	virtual void BuildSamplingInfo(
		TextureSetDefinitionSamplingInfo& SamplingInfo,
		const UMaterialExpressionTextureSetSampleParameter* SampleExpression);

	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) override;

	virtual void GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
		FTextureSetMaterialGraphBuilder& Builder) const override;
};
