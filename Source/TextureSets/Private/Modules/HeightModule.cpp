// (c) Electronic Arts. All Rights Reserved.

#include "Modules/HeightModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

TArray<TextureSetTextureDef> UHeightModule::GetSourceTextures() const
{
	TArray<TextureSetTextureDef> SourceMaps;
	SourceMaps.Add(TextureSetTextureDef{"Height", false, 1, FVector4(1, 0, 0, 0)});
	return SourceMaps;
};

void UHeightModule::CollectShaderConstants(TMap<FName, EMaterialValueType>& Constants, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UHeightSampleParams* HeightSampleParams = SampleExpression->GetSampleParams<UHeightSampleParams>();

	if (HeightSampleParams->bEnableParallaxOcclusionMapping)
	{
		Constants.Add("ParallaxReferencePlane", EMaterialValueType::MCT_Float1);
	}
}

void UHeightModule::CollectSampleInputs(TMap<FName, EMaterialValueType>& Arguments, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const 
{
	const UHeightSampleParams* HeightSampleParams = SampleExpression->GetSampleParams<UHeightSampleParams>();

	if (HeightSampleParams->bEnableParallaxOcclusionMapping)
	{
		Arguments.Add("ParallaxStrength", EMaterialValueType::MCT_Float1);
		Arguments.Add("ParallaxIterations", EMaterialValueType::MCT_Float1);
	}
}

void UHeightModule::CollectSampleOutputs(TMap<FName, EMaterialValueType>& Results, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UHeightSampleParams* HeightSampleParams = SampleExpression->GetSampleParams<UHeightSampleParams>();

	Results.Add("Height", EMaterialValueType::MCT_Float1);

	if (HeightSampleParams->bEnableParallaxOcclusionMapping)
	{
		Results.Add("ParallaxOffset", EMaterialValueType::MCT_Float2);
	}
}

void UHeightModule::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	TObjectPtr<UMaterialExpression> TextureExpression = Builder.GetProcessedTextureSample("Height");
	TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = Builder.GetOutput("Height");
	TextureExpression->ConnectExpression(OutputExpression->GetInput(0), 0);
}