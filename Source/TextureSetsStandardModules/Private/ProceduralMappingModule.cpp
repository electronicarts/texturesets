// Copyright (c) Electronic Arts. All Rights Reserved.


#include "ProceduralMappingModule.h"

#include "ProceduralMappingSampleParams.h"
#include "TextureSetMaterialGraphBuilder.h"
#include "HLSLFunctionCallNodeBuilder.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDDX.h"
#include "Materials/MaterialExpressionDDY.h"

TSubclassOf<UTextureSetSampleParams> UProceduralMappingModule::GetSampleParamClass() const
{
	return UProceduralMappingSampleParams::StaticClass();
}

#if WITH_EDITOR
void UProceduralMappingModule::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams, FTextureSetMaterialGraphBuilder* Builder) const
{
	const UProceduralMappingSampleParams* ProceduralMappingSampleParams = SampleParams->Get<UProceduralMappingSampleParams>();

	if (ProceduralMappingSampleParams->TriplanarMapping == ETriplanarMappingMode::Off)
		return;

	UMaterialExpressionFunctionInput* FalloffInputExpression = Builder->CreateInput("Falloff", EFunctionInputType::FunctionInput_Scalar);
	FalloffInputExpression->bUsePreviewValueAsDefault = true;
	FalloffInputExpression->Description = "Value between 0 and 1 (0: low falloff, 1: high falloff)";
	UMaterialExpressionFunctionInput* UVWInputExpression = Builder->CreateInput("UVW", EFunctionInputType::FunctionInput_Vector3);
	UVWInputExpression->Description = "The position used for triplanar mapping. Use a position in world space as a default.";

	// Triplanar Mapping

	HLSLFunctionCallNodeBuilder TriplanarUVsFunctionCall("TriplanarUVs", "/Plugin/TextureSets/Triplanar.ush");

	FGraphBuilderOutputAddress Normal = Builder->GetFallbackValue(EGraphBuilderSharedValueType::BaseNormal);
	TriplanarUVsFunctionCall.InArgument("Position", FGraphBuilderOutputAddress(UVWInputExpression, 0));
	TriplanarUVsFunctionCall.InArgument("Normal", Normal);
	TriplanarUVsFunctionCall.InArgument("Falloff", FGraphBuilderOutputAddress(FalloffInputExpression, 0));

	TriplanarUVsFunctionCall.OutArgument("UVX", ECustomMaterialOutputType::CMOT_Float2);
	TriplanarUVsFunctionCall.OutArgument("UVY", ECustomMaterialOutputType::CMOT_Float2);
	TriplanarUVsFunctionCall.OutArgument("UVZ", ECustomMaterialOutputType::CMOT_Float2);
	TriplanarUVsFunctionCall.OutArgument("Weights", ECustomMaterialOutputType::CMOT_Float3);

	UMaterialExpression* TriplanarUVsFunctionCallExp = TriplanarUVsFunctionCall.Build(Builder);

	UMaterialExpressionComponentMask* XYMask = Builder->CreateExpression<UMaterialExpressionComponentMask>();
	XYMask->R = true;
	XYMask->G = true;
	XYMask->B = false;
	XYMask->A = false;
	Builder->Connect(UVWInputExpression, 0, XYMask, 0);
	Builder->SetFallbackValue(FGraphBuilderOutputAddress(XYMask, 0), EGraphBuilderSharedValueType::Texcoord_Raw);

	// Triplanar Tangents

	HLSLFunctionCallNodeBuilder TriplanarTangentsFunctionCall("TriplanarTangents", "/Plugin/TextureSets/Triplanar.ush");

	TriplanarTangentsFunctionCall.InArgument("Normal", Builder->GetFallbackValue(EGraphBuilderSharedValueType::BaseNormal));

	TriplanarTangentsFunctionCall.OutArgument("TangentX", ECustomMaterialOutputType::CMOT_Float3);
	TriplanarTangentsFunctionCall.OutArgument("BitangentX", ECustomMaterialOutputType::CMOT_Float3);
	TriplanarTangentsFunctionCall.OutArgument("TangentY", ECustomMaterialOutputType::CMOT_Float3);
	TriplanarTangentsFunctionCall.OutArgument("BitangentY", ECustomMaterialOutputType::CMOT_Float3);
	TriplanarTangentsFunctionCall.OutArgument("TangentZ", ECustomMaterialOutputType::CMOT_Float3);
	TriplanarTangentsFunctionCall.OutArgument("BitangentZ", ECustomMaterialOutputType::CMOT_Float3);

	UMaterialExpression* TriplanarTangentsFunctionCallExp = TriplanarTangentsFunctionCall.Build(Builder);

	switch (ProceduralMappingSampleParams->TriplanarMapping)
	{
		case ETriplanarMappingMode::Triplanar:
		{
			TArray<FSubSampleDefinition> SubsampleDefinitions;

			UMaterialExpressionComponentMask* XMask = Builder->CreateExpression<UMaterialExpressionComponentMask>();
			XMask->R = true;
			XMask->G = false;
			XMask->B = false;
			XMask->A = false;
			Builder->Connect(TriplanarUVsFunctionCallExp, 4, XMask, 0);
			SubsampleDefinitions.Add(FSubSampleDefinition("X", FGraphBuilderOutputAddress(XMask, 0)));

			UMaterialExpressionComponentMask* YMask = Builder->CreateExpression<UMaterialExpressionComponentMask>();
			YMask->R = false;
			YMask->G = true;
			YMask->B = false;
			YMask->A = false;
			Builder->Connect(TriplanarUVsFunctionCallExp, 4, YMask, 0);
			SubsampleDefinitions.Add(FSubSampleDefinition("Y", FGraphBuilderOutputAddress(YMask, 0)));

			UMaterialExpressionComponentMask* ZMask = Builder->CreateExpression<UMaterialExpressionComponentMask>();
			ZMask->R = false;
			ZMask->G = false;
			ZMask->B = true;
			ZMask->A = false;
			Builder->Connect(TriplanarUVsFunctionCallExp, 4, ZMask, 0);
			SubsampleDefinitions.Add(FSubSampleDefinition("Z", FGraphBuilderOutputAddress(ZMask, 0)));

			TArray<SubSampleHandle> SubsampleHandles = Builder->AddSubsampleGroup(SubsampleDefinitions);

			Builder->AddSampleBuilder(SampleBuilderFunction([this, SubsampleHandles, TriplanarUVsFunctionCallExp, TriplanarTangentsFunctionCallExp, Builder](FTextureSetSubsampleContext& SampleContext)
				{
					int32 RelevantIndex = SubsampleHandles.IndexOfByPredicate([&](const FGuid& guid) { return SampleContext.IsRelevant(guid); });

					SampleContext.SetSharedValue(FGraphBuilderOutputAddress(TriplanarUVsFunctionCallExp, RelevantIndex + 1), EGraphBuilderSharedValueType::Texcoord_Raw);
					SampleContext.SetSharedValue(FGraphBuilderOutputAddress(TriplanarUVsFunctionCallExp, RelevantIndex + 1), EGraphBuilderSharedValueType::Texcoord_Sampling);

					SampleContext.SetSharedValue(FGraphBuilderOutputAddress(TriplanarTangentsFunctionCallExp, RelevantIndex * 2 + 1), EGraphBuilderSharedValueType::Tangent);
					SampleContext.SetSharedValue(FGraphBuilderOutputAddress(TriplanarTangentsFunctionCallExp, RelevantIndex * 2 + 2), EGraphBuilderSharedValueType::Bitangent);


					UMaterialExpressionDDX* DDXExp = Builder->CreateExpression<UMaterialExpressionDDX>();
					UMaterialExpressionDDY* DDYExp = Builder->CreateExpression<UMaterialExpressionDDY>();

					Builder->Connect(TriplanarUVsFunctionCallExp, RelevantIndex + 1, DDXExp, 0);
					Builder->Connect(TriplanarUVsFunctionCallExp, RelevantIndex + 1, DDYExp, 0);

					SampleContext.SetSharedValue(FGraphBuilderOutputAddress(DDXExp, 0), EGraphBuilderSharedValueType::Texcoord_DDX);
					SampleContext.SetSharedValue(FGraphBuilderOutputAddress(DDYExp, 0), EGraphBuilderSharedValueType::Texcoord_DDY);

				}));
			break;
		}
		case ETriplanarMappingMode::SingleSample:
		{
			HLSLFunctionCallNodeBuilder NoiseFunctionCall("SingleSampleNoise", "/Plugin/TextureSets/Triplanar.ush");
			HLSLFunctionCallNodeBuilder SingleSampleTriplanarFunctionCall("SingleSampleTriplanar", "/Plugin/TextureSets/Triplanar.ush");

			NoiseFunctionCall.InArgument("Normal", Normal);
			NoiseFunctionCall.OutArgument("Noise", ECustomMaterialOutputType::CMOT_Float3);
			UMaterialExpression* NoiseFunctionCallExp = NoiseFunctionCall.Build(Builder);

			SingleSampleTriplanarFunctionCall.InArgument("UVX", FGraphBuilderOutputAddress(TriplanarUVsFunctionCallExp, 1));
			SingleSampleTriplanarFunctionCall.InArgument("UVY", FGraphBuilderOutputAddress(TriplanarUVsFunctionCallExp, 2));
			SingleSampleTriplanarFunctionCall.InArgument("UVZ", FGraphBuilderOutputAddress(TriplanarUVsFunctionCallExp, 3));

			SingleSampleTriplanarFunctionCall.InArgument("TangentX", FGraphBuilderOutputAddress(TriplanarTangentsFunctionCallExp, 1));
			SingleSampleTriplanarFunctionCall.InArgument("BitangentX", FGraphBuilderOutputAddress(TriplanarTangentsFunctionCallExp, 2));
			SingleSampleTriplanarFunctionCall.InArgument("TangentY", FGraphBuilderOutputAddress(TriplanarTangentsFunctionCallExp, 3));
			SingleSampleTriplanarFunctionCall.InArgument("BitangentY", FGraphBuilderOutputAddress(TriplanarTangentsFunctionCallExp, 4));
			SingleSampleTriplanarFunctionCall.InArgument("TangentZ", FGraphBuilderOutputAddress(TriplanarTangentsFunctionCallExp, 5));
			SingleSampleTriplanarFunctionCall.InArgument("BitangentZ", FGraphBuilderOutputAddress(TriplanarTangentsFunctionCallExp, 6));

			SingleSampleTriplanarFunctionCall.InArgument("Weights", FGraphBuilderOutputAddress(TriplanarUVsFunctionCallExp, 4));
			SingleSampleTriplanarFunctionCall.InArgument("Noise", FGraphBuilderOutputAddress(NoiseFunctionCallExp, 1));

			SingleSampleTriplanarFunctionCall.OutArgument("UVs", ECustomMaterialOutputType::CMOT_Float2);
			SingleSampleTriplanarFunctionCall.OutArgument("Tangent", ECustomMaterialOutputType::CMOT_Float3);
			SingleSampleTriplanarFunctionCall.OutArgument("Bitangent", ECustomMaterialOutputType::CMOT_Float3);
			SingleSampleTriplanarFunctionCall.OutArgument("DDX", ECustomMaterialOutputType::CMOT_Float2);
			SingleSampleTriplanarFunctionCall.OutArgument("DDY", ECustomMaterialOutputType::CMOT_Float2);

			UMaterialExpression* SingleSampleTriplanarFunctionCallExp = SingleSampleTriplanarFunctionCall.Build(Builder);

			Builder->AddSampleBuilder(SampleBuilderFunction([this, SampleParams, SingleSampleTriplanarFunctionCallExp, Builder](FTextureSetSubsampleContext& SampleContext)
				{
					SampleContext.SetSharedValue(FGraphBuilderOutputAddress(SingleSampleTriplanarFunctionCallExp, 1), EGraphBuilderSharedValueType::Texcoord_Raw);
					SampleContext.SetSharedValue(FGraphBuilderOutputAddress(SingleSampleTriplanarFunctionCallExp, 1), EGraphBuilderSharedValueType::Texcoord_Sampling);

					SampleContext.SetSharedValue(FGraphBuilderOutputAddress(SingleSampleTriplanarFunctionCallExp, 2), EGraphBuilderSharedValueType::Tangent);
					SampleContext.SetSharedValue(FGraphBuilderOutputAddress(SingleSampleTriplanarFunctionCallExp, 3), EGraphBuilderSharedValueType::Bitangent);

					SampleContext.SetSharedValue(FGraphBuilderOutputAddress(SingleSampleTriplanarFunctionCallExp, 4), EGraphBuilderSharedValueType::Texcoord_DDX);
					SampleContext.SetSharedValue(FGraphBuilderOutputAddress(SingleSampleTriplanarFunctionCallExp, 5), EGraphBuilderSharedValueType::Texcoord_DDY);
				}));
			break;
		}
	}
}
#endif