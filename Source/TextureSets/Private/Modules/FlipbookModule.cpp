// (c) Electronic Arts. All Rights Reserved.

#include "Modules/FlipbookModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"

void UFlipbookModule::BuildSharedInfo(TextureSetDefinitionSharedInfo& Info) const
{
	if (bUseMotionVectors)
	{
		Info.AddSourceTexture(TextureSetTextureDef{"MotionVector", false, 2, FVector4(0.5, 0.5, 0, 0)});
	}
}

int32 UFlipbookModule::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UFlipbookSampleParams* HeightSampleParams = SampleExpression->GetSampleParams<UFlipbookSampleParams>();

	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);

	Hash = HashCombine(Hash, GetTypeHash(HeightSampleParams->bBlendFrames));
	Hash = HashCombine(Hash, GetTypeHash(bUseMotionVectors)); 

	return Hash;
}

void UFlipbookModule::BuildSamplingInfo(TextureSetDefinitionSamplingInfo& SamplingInfo, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UFlipbookSampleParams* FlipbookSampleParams = SampleExpression->GetSampleParams<UFlipbookSampleParams>();

	SamplingInfo.AddMaterialParameter("FrameCount", EMaterialValueType::MCT_Float1);
	SamplingInfo.AddMaterialParameter("FrameRate", EMaterialValueType::MCT_Float1);

	SamplingInfo.AddSampleInput("FlipbookTime", EMaterialValueType::MCT_Float1);

	if (FlipbookSampleParams->bBlendFrames)
	{
		SamplingInfo.AddSampleOutput("FlipbookUVCurrent", EMaterialValueType::MCT_Float2);
		SamplingInfo.AddSampleOutput("FlipbookUVNext", EMaterialValueType::MCT_Float2);
		SamplingInfo.AddSampleOutput("FlipbookBlend", EMaterialValueType::MCT_Float1);
	}
	else
	{
		SamplingInfo.AddSampleOutput("FlipbookUV", EMaterialValueType::MCT_Float2);
	}
}
