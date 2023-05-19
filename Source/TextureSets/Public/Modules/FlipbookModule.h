// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetDefinition.h"
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
class UFlipbookModule : public UTextureSetDefinitionModule
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere);
	bool bUseMotionVectors = false;

	virtual bool AllowMultiple() override { return false; }
	virtual TSubclassOf<UTextureSetAssetParams> GetAssetParamClass() const { return UFlipbookAssetParams::StaticClass(); }
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const override { return UFlipbookSampleParams::StaticClass(); }
	
	virtual TArray<TextureSetTextureDef> GetSourceTextures() const override;
	virtual void CollectShaderConstants(TMap<FName, EMaterialValueType>& Constants, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;
	virtual void CollectSampleInputs(TMap<FName, EMaterialValueType>& Arguments, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;
	virtual void CollectSampleOutputs(TMap<FName, EMaterialValueType>& Results, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;
};
