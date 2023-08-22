// (c) Electronic Arts. All Rights Reserved.

#include "Modules/FlipbookModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"


void UFlipbookModule::GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	if (bUseMotionVectors)
	{
		FTextureSetSourceTextureDef MotionDef = FTextureSetSourceTextureDef{false, 2, FVector4(0.5, 0.5, 0, 0)};
		Graph.AddOutputTexture("MotionVector", Graph.AddInputTexture("MotionVector", MotionDef));
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
