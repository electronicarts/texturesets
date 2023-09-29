// (c) Electronic Arts. All Rights Reserved.

#include "Modules/HeightModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "ProcessingNodes/ParameterPassthrough.h"

#if WITH_EDITOR
void UHeightModule::GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	const FTextureSetSourceTextureDef HeightDef(1, false, FVector4(1, 0, 0, 0));

	class HeightmapParametersPassThrough : public ParameterPassThrough<UHeightAssetParams>
	{
	public:
		HeightmapParametersPassThrough() : ParameterPassThrough<UHeightAssetParams>("HeightParams", 1) {}

		virtual FVector4f GetValue(const UHeightAssetParams* Parameter) const override
		{
			return FVector4f(
				Parameter->HeightmapScale,
				Parameter->HeightmapReferencePlane,
				0,
				0);
		};
	};

	Graph.AddOutputParameter("HeightParams", TSharedRef<HeightmapParametersPassThrough>(new HeightmapParametersPassThrough()));

	Graph.AddOutputTexture("Height", Graph.AddInputTexture("Height", HeightDef));
}
#endif

#if WITH_EDITOR
int32 UHeightModule::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UHeightSampleParams* HeightSampleParams = SampleExpression->SampleParams.Get<UHeightSampleParams>();

	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);

	Hash = HashCombine(Hash, GetTypeHash("HeightModuleV1"));
	Hash = HashCombine(Hash, GetTypeHash(HeightSampleParams->bEnableParallaxOcclusionMapping));

	return Hash;
}
#endif

#if WITH_EDITOR
void UHeightModule::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	const UHeightSampleParams* HeightSampleParams = SampleExpression->SampleParams.Get<UHeightSampleParams>();

	TObjectPtr<UMaterialExpression> TextureExpression = Builder.GetProcessedTextureSample("Height");
	TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = Builder.CreateOutput("Height");
	TextureExpression->ConnectExpression(OutputExpression->GetInput(0), 0);

	if (HeightSampleParams->bEnableParallaxOcclusionMapping)
	{
		UMaterialExpression* HeightParams = Builder.MakeConstantParameter("HeightParams", FVector4f::One());

		UMaterialExpressionMultiply* HeightScale = Builder.CreateExpression<UMaterialExpressionMultiply>();
		{
			// Reading the parameter that comes from UHeightAssetParams::HeightmapScale
			UMaterialExpressionComponentMask* HeightScaleParam = Builder.CreateExpression<UMaterialExpressionComponentMask>();
			HeightParams->ConnectExpression(HeightScaleParam->GetInput(0), 0);
			HeightScaleParam->R = true;
			HeightScaleParam->G = false;
			HeightScaleParam->B = false;
			HeightScaleParam->A = false;

			UMaterialExpressionFunctionInput* StrengthInput = Builder.CreateInput("HeightModifier", EFunctionInputType::FunctionInput_Scalar);
			StrengthInput->PreviewValue = FVector4f(1.0f);
			StrengthInput->bUsePreviewValueAsDefault = true;

			HeightScaleParam->ConnectExpression(HeightScale->GetInput(0), 0);
			StrengthInput->ConnectExpression(HeightScale->GetInput(1), 0);
		}

		UMaterialExpressionAdd* ReferencePlane = Builder.CreateExpression<UMaterialExpressionAdd>();
		{
			// Reading the parameter that comes from UHeightAssetParams::HeightmapReferencePlane
			UMaterialExpressionComponentMask* ReferencePlaneParam = Builder.CreateExpression<UMaterialExpressionComponentMask>();
			HeightParams->ConnectExpression(ReferencePlaneParam->GetInput(0), 0);
			ReferencePlaneParam->R = false;
			ReferencePlaneParam->G = true;
			ReferencePlaneParam->B = false;
			ReferencePlaneParam->A = false;
			
			UMaterialExpressionFunctionInput* ReferencePlaneOffsetInput = Builder.CreateInput("ReferencePlaneOffset", EFunctionInputType::FunctionInput_Scalar);
			ReferencePlaneOffsetInput->PreviewValue = FVector4f(0.0f);
			ReferencePlaneOffsetInput->bUsePreviewValueAsDefault = true;
		
			ReferencePlaneParam->ConnectExpression(ReferencePlane->GetInput(0), 0);
			ReferencePlaneOffsetInput->ConnectExpression(ReferencePlane->GetInput(1), 0);
		}

		UMaterialExpressionFunctionInput* IterationsInput = Builder.CreateInput("ParallaxIterations", EFunctionInputType::FunctionInput_Scalar);
		IterationsInput->PreviewValue = FVector4f(16.0f);
		IterationsInput->bUsePreviewValueAsDefault = true;

		auto [PackedTextureIndex, PackedTextureChannel] = Builder.GetPackingSource("Height.r");

		HLSLFunctionCallNodeBuilder FunctionCall("ParallaxOcclusionMapping", "/Plugin/TextureSets/Parallax.ush");
		
		FunctionCall.SetReturnType(ECustomMaterialOutputType::CMOT_Float2);

		FunctionCall.InArgument("Texcoord", Builder.GetRawTexcoord(), 0);
		FunctionCall.InArgument("WSNormal", Builder.GetBaseNormal(), 0);
		FunctionCall.InArgument("WSView", Builder.GetCameraVector(), 0);
		FunctionCall.InArgument("Strength", HeightScale, 0);
		FunctionCall.InArgument("ReferencePlane", ReferencePlane, 0);
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
