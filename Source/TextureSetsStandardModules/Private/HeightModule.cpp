// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "HeightModule.h"

#include "TextureSetProcessingGraph.h"
#include "TextureSetMaterialGraphBuilder.h"
#include "TextureSetDefinition.h"
#include "HLSLFunctionCallNodeBuilder.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Misc/DataValidation.h"
#include "ProcessingNodes/ParameterPassthrough.h"
#include "ProcessingNodes/TextureInput.h"

#define LOCTEXT_NAMESPACE "TextureSets"

void UHeightModule::GetAssetParamClasses(TSet<TSubclassOf<UTextureSetAssetParams>>& Classes) const
{
	Classes.Add(UHeightAssetParams::StaticClass());
}

void UHeightModule::GetSampleParamClasses(TSet<TSubclassOf<UTextureSetSampleParams>>& Classes) const
{
	Classes.Add(UHeightSampleParams::StaticClass());
}

void UHeightModule::ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	// Create a def for our source texture
	FTextureSetSourceTextureDef HeightDef;
	HeightDef.ChannelCount = 1;
	HeightDef.Encoding = (uint8)ETextureSetChannelEncoding::RangeCompression;
	HeightDef.DefaultValue = FVector4(1, 0, 0, 0);
	HeightDef.Flags = (uint8)ETextureSetTextureFlags::Default;

	// Create input texture based on height def
	TSharedRef<FTextureInput> TextureInput = Graph.AddInputTexture("Height", HeightDef);

	// Output the texture without further modification
	Graph.AddOutputTexture("Height", TextureInput);

	// Output 
	Graph.AddOutputParameter("HeightParams", MakeShared<ParameterPassThrough<UHeightAssetParams>>(
		"HeightParams", 
		[](const UHeightAssetParams* Parameter) -> FVector4f
		{
			return FVector4f(
				Parameter->HeightmapScale,
				Parameter->HeightmapReferencePlane,
				0,
				0);
		}
		));
}

int32 UHeightModule::ComputeSamplingHash(const FTextureSetAssetParamsCollection* SampleParams) const
{
	const UHeightSampleParams* HeightSampleParams = SampleParams->Get<UHeightSampleParams>();

	uint32 Hash = Super::ComputeSamplingHash(SampleParams);

	static const char* name = "HeightModuleV2";
	Hash = HashCombine(Hash, GetArrayHash(name, sizeof(name)));
	Hash = HashCombine(Hash, GetTypeHash(HeightSampleParams->bEnableParallaxOcclusionMapping));

	return Hash;
}

void UHeightModule::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
	FTextureSetMaterialGraphBuilder* Builder) const
{
	const UHeightSampleParams* HeightSampleParams = SampleParams->Get<UHeightSampleParams>();

	if (HeightSampleParams->bEnableParallaxOcclusionMapping)
	{
		FGraphBuilderOutputAddress HeightParams = FGraphBuilderOutputAddress(Builder->MakeConstantParameter("HeightParams", FVector4f::One()), 0);

		struct
		{
			TTuple<int, int> HeightPackingSource;
			FGraphBuilderOutputAddress HeightScale;
			FGraphBuilderOutputAddress ReferencePlane;
			FGraphBuilderOutputAddress Iterations;
		} POMSampleBuilderParams;
		
		POMSampleBuilderParams.HeightPackingSource = Builder->GetPackingSource("Height.r");

		// HeightScale
		{
			// Reading the parameter that comes from UHeightAssetParams::HeightmapScale
			UMaterialExpressionComponentMask* HeightScaleMask = Builder->CreateExpression<UMaterialExpressionComponentMask>();
			HeightScaleMask->R = true;
			HeightScaleMask->G = false;
			HeightScaleMask->B = false;
			HeightScaleMask->A = false;
			Builder->Connect(HeightParams, HeightScaleMask, 0);

			UMaterialExpressionFunctionInput* StrengthInput = Builder->CreateInput("HeightModifier", EFunctionInputType::FunctionInput_Scalar);
			StrengthInput->PreviewValue = FVector4f(1.0f);
			StrengthInput->bUsePreviewValueAsDefault = true;

			UMaterialExpressionMultiply* Multiply = Builder->CreateExpression<UMaterialExpressionMultiply>();
			Builder->Connect(HeightScaleMask, 0, Multiply, 0);
			Builder->Connect(StrengthInput, 0, Multiply, 1);

			POMSampleBuilderParams.HeightScale = FGraphBuilderOutputAddress(Multiply, 0);
		}
		
		// ReferencePlane
		{
			// Reading the parameter that comes from UHeightAssetParams::HeightmapReferencePlane
			UMaterialExpressionComponentMask* ReferencePlaneMask = Builder->CreateExpression<UMaterialExpressionComponentMask>();
			ReferencePlaneMask->R = false;
			ReferencePlaneMask->G = true;
			ReferencePlaneMask->B = false;
			ReferencePlaneMask->A = false;
			Builder->Connect(HeightParams, ReferencePlaneMask, 0);

			UMaterialExpressionFunctionInput* ReferencePlaneOffsetInput = Builder->CreateInput("ReferencePlaneOffset", EFunctionInputType::FunctionInput_Scalar);
			ReferencePlaneOffsetInput->PreviewValue = FVector4f(0.0f);
			ReferencePlaneOffsetInput->bUsePreviewValueAsDefault = true;

			UMaterialExpressionAdd* Add = Builder->CreateExpression<UMaterialExpressionAdd>();
			Builder->Connect(ReferencePlaneMask, 0, Add, 0);
			Builder->Connect(ReferencePlaneOffsetInput, 0, Add, 1);

			POMSampleBuilderParams.ReferencePlane = FGraphBuilderOutputAddress(Add, 0);
		}

		// Iterations
		{
			UMaterialExpressionFunctionInput* IterationsInput = Builder->CreateInput("ParallaxIterations", EFunctionInputType::FunctionInput_Scalar);
			IterationsInput->PreviewValue = FVector4f(16.0f);
			IterationsInput->bUsePreviewValueAsDefault = true;

			POMSampleBuilderParams.Iterations = FGraphBuilderOutputAddress(IterationsInput, 0);
		}

		Builder->AddSampleBuilder(SampleBuilderFunction([this, POMSampleBuilderParams, Builder](FTextureSetSubsampleContext& SampleContext)
		{
			const uint32 PackedTextureIndex = POMSampleBuilderParams.HeightPackingSource.Key;
			const uint32 PackedTextureChannel = POMSampleBuilderParams.HeightPackingSource.Value;

			auto [RangeCompressMul, RangeCompressAdd] = Builder->GetRangeCompressParams(PackedTextureIndex);

			HLSLFunctionCallNodeBuilder FunctionCall("ParallaxOcclusionMapping", "/Plugin/TextureSets/Parallax.ush");

			FunctionCall.SetReturnType(ECustomMaterialOutputType::CMOT_Float2);

			FunctionCall.InArgument("Texcoord", SampleContext.GetSharedValue(EGraphBuilderSharedValueType::Texcoord_Raw));
			FunctionCall.InArgument("WSNormal", SampleContext.GetSharedValue(EGraphBuilderSharedValueType::BaseNormal));
			FunctionCall.InArgument("WSView", SampleContext.GetSharedValue(EGraphBuilderSharedValueType::CameraVector));
			FunctionCall.InArgument("Strength", POMSampleBuilderParams.HeightScale);
			FunctionCall.InArgument("ReferencePlane", POMSampleBuilderParams.ReferencePlane);
			FunctionCall.InArgument("Position", SampleContext.GetSharedValue(EGraphBuilderSharedValueType::Position));
			FunctionCall.InArgument("SampleCount", POMSampleBuilderParams.Iterations);
			FunctionCall.InArgument("Heightmap", Builder->GetPackedTextureObject(PackedTextureIndex, SampleContext.GetSharedValue(EGraphBuilderSharedValueType::Texcoord_Streaming)));
			FunctionCall.InArgument("HeightmapSampler", "HeightmapSampler");
			FunctionCall.InArgument("HeightmapChannel", FString::FromInt(PackedTextureChannel)); // Hard-coded (in the shader) based on packing
			FunctionCall.InArgument("HeightmapSize", Builder->GetPackedTextureSize(PackedTextureIndex));
			FunctionCall.InArgument("Tangent", SampleContext.GetSharedValue(EGraphBuilderSharedValueType::Tangent));
			FunctionCall.InArgument("Bitangent", SampleContext.GetSharedValue(EGraphBuilderSharedValueType::Bitangent));
			FunctionCall.InArgument("RangeCompressMul", RangeCompressMul);
			FunctionCall.InArgument("RangeCompressAdd", RangeCompressAdd);

			FunctionCall.OutArgument("TexcoordOffset", ECustomMaterialOutputType::CMOT_Float2);
			FunctionCall.OutArgument("DepthOffset", ECustomMaterialOutputType::CMOT_Float1);
			FunctionCall.OutArgument("Height", ECustomMaterialOutputType::CMOT_Float1);

			UMaterialExpression* FunctionCallExp = FunctionCall.Build(Builder);

			// Modify the sampling texcoord for this subsample with the offset texture coordinate
			SampleContext.SetSharedValue(FGraphBuilderOutputAddress(FunctionCallExp, 0), EGraphBuilderSharedValueType::Texcoord_Sampling);

			// Output height directly from the POM node, to save another sample
			SampleContext.AddResult("Height", FGraphBuilderOutputAddress(FunctionCallExp, 3));

			if (SampleContext.GetAddress().IsRoot())
			{
				// Output the parallax offset result if we're just doing one sample, as it doesn't make sense to blend
				SampleContext.AddResult("ParallaxOffset", FGraphBuilderOutputAddress(FunctionCallExp, 1));
			}
		}));
	}
	else // !HeightSampleParams->bEnableParallaxOcclusionMapping
	{
		// If not doing POM, just add a simple sample builder that just outputs height.
		Builder->AddSampleBuilder(SampleBuilderFunction([this, Builder](FTextureSetSubsampleContext& SampleContext)
		{
			SampleContext.AddResult("Height", SampleContext.GetProcessedTextureSample("Height"));
		}));
	}
}

EDataValidationResult UHeightModule::IsDefinitionValid(const UTextureSetDefinition* Definition,
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;
	const FTextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();

	const int32 HeightPackedTextureIdx = PackingInfo.GetPackingSource("Height.r").Key;
	const bool bVirtualTextureStreaming = PackingInfo.GetPackedTextureDef(HeightPackedTextureIdx).bVirtualTextureStreaming;

	if (bVirtualTextureStreaming)
	{
		Context.AddError(LOCTEXT("NoVTHeight", "Height cannot be a channel in a derived texture that is tagged as a virtual texture."));
		Result = EDataValidationResult::Invalid;
	}

	return CombineDataValidationResults(Result, Super::IsDefinitionValid(Definition, Context));
}

#undef LOCTEXT_NAMESPACE