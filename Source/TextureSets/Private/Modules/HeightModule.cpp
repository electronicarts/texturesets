// (c) Electronic Arts. All Rights Reserved.

#include "Modules/HeightModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

void UHeightModule::BuildSharedInfo(TextureSetDefinitionSharedInfo& Info) const
{
	TextureSetTextureDef HeightDef = {"Height", false, 1, FVector4(1, 0, 0, 0)};

	Info.AddSourceTexture(HeightDef);
	Info.AddProcessedTexture(HeightDef);
}

void UHeightModule::BuildSamplingInfo(TextureSetDefinitionSamplingInfo& SamplingInfo, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UHeightSampleParams* HeightSampleParams = SampleExpression->GetSampleParams<UHeightSampleParams>();

	if (HeightSampleParams->bEnableParallaxOcclusionMapping)
	{
		SamplingInfo.AddMaterialParameter("ParallaxReferencePlane", EMaterialValueType::MCT_Float1);
		SamplingInfo.AddSampleInput("ParallaxStrength", EMaterialValueType::MCT_Float1);
		SamplingInfo.AddSampleInput("ParallaxIterations", EMaterialValueType::MCT_Float1);
		SamplingInfo.AddSampleOutput("ParallaxOffset", EMaterialValueType::MCT_Float2);
	}

	SamplingInfo.AddSampleOutput("Height", EMaterialValueType::MCT_Float1);
}

void UHeightModule::Process(FTextureSetProcessingContext& Context) const
{
	Context.AddProcessedTexture("Height", Context.GetSourceTexture("Height"));
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
	TObjectPtr<UMaterialExpression> TextureExpression = Builder.GetProcessedTextureSample("Height");
	TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = Builder.GetOutput("Height");
	TextureExpression->ConnectExpression(OutputExpression->GetInput(0), 0);
}
#endif
