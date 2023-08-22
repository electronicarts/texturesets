// (c) Electronic Arts. All Rights Reserved.

#include "Modules/HeightModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

void UHeightModule::GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	FTextureSetSourceTextureDef HeightDef = {false, 1, FVector4(1, 0, 0, 0)};

	Graph.AddOutputTexture("Height", Graph.AddInputTexture("Height", HeightDef));
}

int32 UHeightModule::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UHeightSampleParams* HeightSampleParams = SampleExpression->GetSampleParams<UHeightSampleParams>();

	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);

	Hash = HashCombine(Hash, GetTypeHash(HeightSampleParams->bEnableParallaxOcclusionMapping));

	return Hash;
}

#if WITH_EDITOR
void UHeightModule::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	//const UHeightSampleParams* HeightSampleParams = SampleExpression->GetSampleParams<UHeightSampleParams>();

	//if (HeightSampleParams->bEnableParallaxOcclusionMapping)
	//{
	//	Builder.AddMaterialParameter("ParallaxReferencePlane", EMaterialValueType::MCT_Float1);
	//	Builder.AddSampleInput("ParallaxStrength", EMaterialValueType::MCT_Float1);
	//	Builder.AddSampleInput("ParallaxIterations", EMaterialValueType::MCT_Float1);
	//	Builder.AddSampleOutput("ParallaxOffset", EMaterialValueType::MCT_Float2);
	//}

	TObjectPtr<UMaterialExpression> TextureExpression = Builder.GetProcessedTextureSample("Height");
	TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = Builder.CreateOutput("Height");
	TextureExpression->ConnectExpression(OutputExpression->GetInput(0), 0);
}
#endif
