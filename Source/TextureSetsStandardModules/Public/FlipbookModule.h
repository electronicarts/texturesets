// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "FlipbookModule.generated.h"

UENUM()
enum class EFlipbookSourceType : uint8
{
	TextureArray,
	TextureSheet
};

UCLASS()
class UFlipbookAssetParams : public UTextureSetAssetParams
{
	GENERATED_BODY()
public:
	// Default flipbook framerate, in frames per second. Used when flipbook frame interpolation time is set to "seconds".
	UPROPERTY(EditAnywhere, Category="Flipbook")
	float FlipbookFramerate = 15.0f;

	// Is this a looping flipbook animation, or a one-off.
	UPROPERTY(EditAnywhere, Category="Flipbook")
	bool bFlipbookLooping = true;

	// Scale applied to the motion vectors, if they're enabled.
	UPROPERTY(EditAnywhere, Category="Flipbook")
	FVector2f MotionVectorScale {1.0f, 1.0f};

	UPROPERTY(EditAnywhere, Category="Flipbook")
	EFlipbookSourceType FlipbookSourceType = EFlipbookSourceType::TextureArray;

	UPROPERTY(EditAnywhere, Category="Flipbook", meta=(EditCondition ="FlipbookSourceType == EFlipbookSourceType::TextureSheet"))
	uint32 FlipbookSourceSheetWidth = 4;

	UPROPERTY(EditAnywhere, Category="Flipbook", meta=(EditCondition ="FlipbookSourceType == EFlipbookSourceType::TextureSheet"))
	uint32 FlipbookSourceSheetHeight = 4;
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
	UPROPERTY(EditAnywhere, Category="Flipbook");
	bool bBlendFrames = true;

	UPROPERTY(EditAnywhere, Category="Flipbook");
	EFlipbookTime FlipbookTimeType = EFlipbookTime::Seconds;
};

// Converts all texture elements into arrays, allowing flipbooking of animated textures.
UCLASS()
class UFlipbookModule : public UTextureSetModule
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Flipbook");
	bool bUseMotionVectors = false;

	virtual TSubclassOf<UTextureSetAssetParams> GetAssetParamClass() const { return UFlipbookAssetParams::StaticClass(); }
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const override { return UFlipbookSampleParams::StaticClass(); }
	
	virtual void ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual int32 ComputeSamplingHash(const FTextureSetAssetParamsCollection* SampleParams) const override;

	virtual void ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
		FTextureSetMaterialGraphBuilder* Builder) const override;

	virtual EDataValidationResult IsDefinitionValid(const UTextureSetDefinition* Definition, FDataValidationContext& Context) const override;
};
