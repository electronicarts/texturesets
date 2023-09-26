// (c) Electronic Arts. All Rights Reserved.

#include "Modules/HeightModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"

#if WITH_EDITOR
void UHeightModule::GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	const FTextureSetSourceTextureDef HeightDef(1, false, FVector4(1, 0, 0, 0));

	Graph.AddOutputTexture("Height", Graph.AddInputTexture("Height", HeightDef));
}
#endif

#if WITH_EDITOR
int32 UHeightModule::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UHeightSampleParams* HeightSampleParams = SampleExpression->GetSampleParams<UHeightSampleParams>();

	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);

	Hash = HashCombine(Hash, GetTypeHash(HeightSampleParams->bEnableParallaxOcclusionMapping));

	return Hash;
}
#endif

#if WITH_EDITOR
void UHeightModule::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	const UHeightSampleParams* HeightSampleParams = SampleExpression->GetSampleParams<UHeightSampleParams>();

	TObjectPtr<UMaterialExpression> TextureExpression = Builder.GetProcessedTextureSample("Height");
	TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = Builder.CreateOutput("Height");
	TextureExpression->ConnectExpression(OutputExpression->GetInput(0), 0);

	if (HeightSampleParams->bEnableParallaxOcclusionMapping)
	{
		UMaterialExpressionFunctionInput* ReferencePlaneInput = Builder.CreateInput("ParallaxReferencePlane", EFunctionInputType::FunctionInput_Scalar);
		ReferencePlaneInput->PreviewValue = FVector4f(1.0f);
		ReferencePlaneInput->bUsePreviewValueAsDefault = true;

		UMaterialExpressionFunctionInput* StrengthInput = Builder.CreateInput("ParallaxStrength", EFunctionInputType::FunctionInput_Scalar);
		StrengthInput->PreviewValue = FVector4f(0.05f);
		StrengthInput->bUsePreviewValueAsDefault = true;

		UMaterialExpressionFunctionInput* IterationsInput = Builder.CreateInput("ParallaxIterations", EFunctionInputType::FunctionInput_Scalar);
		IterationsInput->PreviewValue = FVector4f(16.0f);
		IterationsInput->bUsePreviewValueAsDefault = true;

		auto [PackedTextureIndex, PackedTextureChannel] = Builder.GetPackingSource("Height.r");

		HLSLFunctionCallNodeBuilder FunctionCall("ParallaxOcclusionMapping", "/Plugin/TextureSets/Parallax.ush");
		
		FunctionCall.SetReturnType(ECustomMaterialOutputType::CMOT_Float2);

		FunctionCall.InArgument("Texcoord", Builder.GetRawTexcoord(), 0);
		FunctionCall.InArgument("WSNormal", Builder.GetBaseNormal(), 0);
		FunctionCall.InArgument("WSView", Builder.GetCameraVector(), 0);
		FunctionCall.InArgument("Strength", StrengthInput, 0);
		FunctionCall.InArgument("ReferencePlane", ReferencePlaneInput, 0);
		FunctionCall.InArgument("Position", Builder.GetPosition(), 0);
		FunctionCall.InArgument("SampleCount", IterationsInput, 0);
		FunctionCall.InArgument("Heightmap", Builder.GetPackedTextureObject(PackedTextureIndex), 0);
		FunctionCall.InArgument("HeightmapSampler", "HeightmapSampler");
		FunctionCall.InArgument("HeightmapChannel", FString::FromInt(PackedTextureChannel)); // Hard-coded (in the shader) based on packing
		FunctionCall.InArgument("HeightmapSize", Builder.GetPackedTextureSize(PackedTextureIndex), 0);
		FunctionCall.InArgument("Tangent", Builder.GetTangent(), 0);
		FunctionCall.InArgument("Bitangent", Builder.GetBitangent(), 0);

		FunctionCall.OutArgument("TexcoordOffset", ECustomMaterialOutputType::CMOT_Float2);
		FunctionCall.OutArgument("DepthOffset", ECustomMaterialOutputType::CMOT_Float1);
		FunctionCall.OutArgument("Height", ECustomMaterialOutputType::CMOT_Float1);

		UMaterialExpression* FunctionCallExp = FunctionCall.Build(Builder);

		Builder.OverrideTexcoord(FunctionCallExp, false, false);

		UMaterialExpressionFunctionOutput* OffsetOutput = Builder.CreateOutput("ParallaxOffset");
		FunctionCallExp->ConnectExpression(OffsetOutput->GetInput(0), 1);
	}

}
#endif
