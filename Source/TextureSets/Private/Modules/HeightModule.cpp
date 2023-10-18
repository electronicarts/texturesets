// (c) Electronic Arts. All Rights Reserved.

#include "Modules/HeightModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialGraphBuilder/HLSLFunctionCallNodeBuilder.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "ProcessingNodes/ParameterPassthrough.h"

#if WITH_EDITOR
void UHeightModule::GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	const FTextureSetSourceTextureDef HeightDef(1, false, FVector4(1, 0, 0, 0));

	Graph.AddOutputParameter("HeightParams", TSharedRef<IParameterProcessingNode>(new ParameterPassThrough<UHeightAssetParams>(
		"HeightParams", 
		[](const UHeightAssetParams* Parameter) -> FVector4f
		{
			return FVector4f(
				Parameter->HeightmapScale,
				Parameter->HeightmapReferencePlane,
				0,
				0);
		}
		)));

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

	Builder.Connect(Builder.GetProcessedTextureSample("Height"), Builder.CreateOutput("Height"), 0);

	if (HeightSampleParams->bEnableParallaxOcclusionMapping)
	{
		FGraphBuilderOutputAddress HeightParams = FGraphBuilderOutputAddress(Builder.MakeConstantParameter("HeightParams", FVector4f::One()), 0);

		FGraphBuilderOutputAddress HeightScale;
		{
			// Reading the parameter that comes from UHeightAssetParams::HeightmapScale
			UMaterialExpressionComponentMask* HeightScaleMask = Builder.CreateExpression<UMaterialExpressionComponentMask>();
			HeightScaleMask->R = true;
			HeightScaleMask->G = false;
			HeightScaleMask->B = false;
			HeightScaleMask->A = false;
			Builder.Connect(HeightParams, HeightScaleMask, 0);

			UMaterialExpressionFunctionInput* StrengthInput = Builder.CreateInput("HeightModifier", EFunctionInputType::FunctionInput_Scalar);
			StrengthInput->PreviewValue = FVector4f(1.0f);
			StrengthInput->bUsePreviewValueAsDefault = true;

			UMaterialExpressionMultiply* Multiply = Builder.CreateExpression<UMaterialExpressionMultiply>();
			Builder.Connect(HeightScaleMask, 0, Multiply, 0);
			Builder.Connect(StrengthInput, 0, Multiply, 1);

			HeightScale = FGraphBuilderOutputAddress(Multiply, 0);
		}

		FGraphBuilderOutputAddress ReferencePlane;
		{
			// Reading the parameter that comes from UHeightAssetParams::HeightmapReferencePlane
			UMaterialExpressionComponentMask* ReferencePlaneMask = Builder.CreateExpression<UMaterialExpressionComponentMask>();
			ReferencePlaneMask->R = false;
			ReferencePlaneMask->G = true;
			ReferencePlaneMask->B = false;
			ReferencePlaneMask->A = false;
			Builder.Connect(HeightParams, ReferencePlaneMask, 0);
			
			UMaterialExpressionFunctionInput* ReferencePlaneOffsetInput = Builder.CreateInput("ReferencePlaneOffset", EFunctionInputType::FunctionInput_Scalar);
			ReferencePlaneOffsetInput->PreviewValue = FVector4f(0.0f);
			ReferencePlaneOffsetInput->bUsePreviewValueAsDefault = true;

			UMaterialExpressionAdd* Add = Builder.CreateExpression<UMaterialExpressionAdd>();
			Builder.Connect(ReferencePlaneMask, 0, Add, 0);
			Builder.Connect(ReferencePlaneOffsetInput, 0, Add, 1);

			ReferencePlane = FGraphBuilderOutputAddress(Add, 0);
		}

		FGraphBuilderOutputAddress Iterations;
		{
			UMaterialExpressionFunctionInput* IterationsInput = Builder.CreateInput("ParallaxIterations", EFunctionInputType::FunctionInput_Scalar);
			IterationsInput->PreviewValue = FVector4f(16.0f);
			IterationsInput->bUsePreviewValueAsDefault = true;

			Iterations = FGraphBuilderOutputAddress(IterationsInput, 0);
		}

		auto [PackedTextureIndex, PackedTextureChannel] = Builder.GetPackingSource("Height.r");

		HLSLFunctionCallNodeBuilder FunctionCall("ParallaxOcclusionMapping", "/Plugin/TextureSets/Parallax.ush");
		
		FunctionCall.SetReturnType(ECustomMaterialOutputType::CMOT_Float2);

		FunctionCall.InArgument("Texcoord", Builder.GetRawUV());
		FunctionCall.InArgument("WSNormal", Builder.GetSharedValue(EGraphBuilderSharedValueType::BaseNormal));
		FunctionCall.InArgument("WSView", Builder.GetSharedValue(EGraphBuilderSharedValueType::CameraVector));
		FunctionCall.InArgument("Strength", HeightScale);
		FunctionCall.InArgument("ReferencePlane", ReferencePlane);
		FunctionCall.InArgument("Position", Builder.GetSharedValue(EGraphBuilderSharedValueType::Position));
		FunctionCall.InArgument("SampleCount", Iterations);
		FunctionCall.InArgument("Heightmap", FGraphBuilderOutputAddress(Builder.GetPackedTextureObject(PackedTextureIndex), 0));
		FunctionCall.InArgument("HeightmapSampler", "HeightmapSampler");
		FunctionCall.InArgument("HeightmapChannel", FString::FromInt(PackedTextureChannel)); // Hard-coded (in the shader) based on packing
		FunctionCall.InArgument("HeightmapSize", Builder.GetPackedTextureSize(PackedTextureIndex));
		FunctionCall.InArgument("Tangent", Builder.GetSharedValue(EGraphBuilderSharedValueType::Tangent));
		FunctionCall.InArgument("Bitangent", Builder.GetSharedValue(EGraphBuilderSharedValueType::Bitangent));

		FunctionCall.OutArgument("TexcoordOffset", ECustomMaterialOutputType::CMOT_Float2);
		FunctionCall.OutArgument("DepthOffset", ECustomMaterialOutputType::CMOT_Float1);
		FunctionCall.OutArgument("Height", ECustomMaterialOutputType::CMOT_Float1);

		UMaterialExpression* FunctionCallExp = FunctionCall.Build(Builder);

		Builder.SetSharedValue(FGraphBuilderOutputAddress(FunctionCallExp, 0), EGraphBuilderSharedValueType::UV_Sampling);

		UMaterialExpressionFunctionOutput* OffsetOutput = Builder.CreateOutput("ParallaxOffset");
		Builder.Connect(FunctionCallExp, 1, OffsetOutput, 0);
	}

}
#endif
