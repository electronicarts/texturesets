// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "FlipbookModule.generated.h"

UCLASS()
class UFlipbookAssetParams : public UTextureSetAssetParams
{
	GENERATED_BODY()
public:
	// Default flipbook framerate, in frames per second. Used when flipbook frame interpolation time is set to "seconds".
	UPROPERTY(EditAnywhere)
	float FlipbookFramerate = 15.0f;

	// Is this a looping flipbook animation, or a one-off.
	UPROPERTY(EditAnywhere)
	bool bFlipbookLooping = true;

	// Scale applied to the motion vectors, if they're enabled.
	UPROPERTY(EditAnywhere)
	float MotionVectorScale = 1.0f;
};


UENUM()
enum class EFlipbookTime : uint8
{
	Seconds UMETA(ToolTip = "Flipbook time is specified in seconds."),
	Frames UMETA(ToolTip = "Flipbook time is specified in whole frames."),
	Normalized UMETA(ToolTip = "Flipbook time is normalized between 0 and 1, where 0 is the first frame and 1 is the last frame."),
};

UCLASS()
class UFlipbookSampleParams : public UTextureSetSampleParams
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere);
	bool bBlendFrames = false;

	UPROPERTY(EditAnywhere);
	EFlipbookTime FlipbookTimeType = EFlipbookTime::Seconds;
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
	
#if WITH_EDITOR
	virtual void ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;

	virtual void ConfigureSamplingGraphBuilder(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
		FTextureSetMaterialGraphBuilder* Builder) const override;
#endif
};
