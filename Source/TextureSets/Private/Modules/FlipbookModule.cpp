// (c) Electronic Arts. All Rights Reserved.

#include "Modules/FlipbookModule.h"
#include "MaterialExpressionTextureSetSampleParameter.h"

TArray<TextureSetTextureDef> UFlipbookModule::GetSourceTextures() const
{
	TArray<TextureSetTextureDef> SourceMaps;
	if (bUseMotionVectors)
	{
		SourceMaps.Add(TextureSetTextureDef{"MotionVector", false, 2, FVector4(0.5, 0.5, 0, 0)});
	}
	return SourceMaps;
}

void UFlipbookModule::CollectShaderConstants(TMap<FName, EMaterialValueType>& Constants, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UFlipbookSampleParams* FlipbookSampleParams = SampleExpression->GetSampleParams<UFlipbookSampleParams>();
	
	Constants.Add("FrameCount", EMaterialValueType::MCT_Float);
	Constants.Add("FrameRate", EMaterialValueType::MCT_Float);
}

void UFlipbookModule::CollectSampleInputs(TMap<FName, EMaterialValueType>& Arguments, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UFlipbookSampleParams* FlipbookSampleParams = SampleExpression->GetSampleParams<UFlipbookSampleParams>();

	Arguments.Add("FlibookTime", EMaterialValueType::MCT_Float);
}

void UFlipbookModule::CollectSampleOutputs(TMap<FName, EMaterialValueType>& Results, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UFlipbookSampleParams* FlipbookSampleParams = SampleExpression->GetSampleParams<UFlipbookSampleParams>();

	if (FlipbookSampleParams->bBlendFrames)
	{
		Results.Add("FlipbookUVCurrent", EMaterialValueType::MCT_Float2);
		Results.Add("FlipbookUVNext", EMaterialValueType::MCT_Float2);
	}
	else
	{
		Results.Add("FlipbookUV", EMaterialValueType::MCT_Float2);
	}
}
