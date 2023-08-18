// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "FlipbookModule.generated.h"

UCLASS()
class UFlipbookAssetParams : public UTextureSetAssetParams
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
	int FlipbookFramecount;

	UPROPERTY(EditAnywhere)
	float FlipbookFramerate;
};

UCLASS()
class UFlipbookSampleParams : public UTextureSetSampleParams
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere);
	bool bBlendFrames = false;
};

UCLASS()
class UFlipbookModule : public UTextureSetModule
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere);
	bool bUseMotionVectors = false;

	virtual bool AllowMultiple() const override { return false; }
	virtual TSubclassOf<UTextureSetAssetParams> GetAssetParamClass() const { return UFlipbookAssetParams::StaticClass(); }
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const override { return UFlipbookSampleParams::StaticClass(); }
	
	virtual void BuildModuleInfo(FTextureSetDefinitionModuleInfo& Info) const;

	virtual void BuildSamplingInfo(
		FTextureSetDefinitionSamplingInfo& SamplingInfo,
		const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const;

	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;
};
