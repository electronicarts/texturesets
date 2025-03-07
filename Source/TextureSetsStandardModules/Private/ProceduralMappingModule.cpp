// Copyright (c) 2024 Electronic Arts. All Rights Reserved.


#include "ProceduralMappingModule.h"

#include "ProceduralMappingSampleParams.h"
#include "TextureSetSampleFunctionBuilder.h"
#include "HLSLFunctionCallNodeBuilder.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDDX.h"
#include "Materials/MaterialExpressionDDY.h"

void UProceduralMappingModule::GetSampleParamClasses(TSet<TSubclassOf<UTextureSetSampleParams>>& Classes) const
{
	Classes.Add(UProceduralMappingSampleParams::StaticClass());
}

void UProceduralMappingModule::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams, FTextureSetSampleFunctionBuilder* Builder) const
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

	FGraphBuilderOutputPin Normal = Builder->GetSharedValue(FSubSampleAddress::Root(), EGraphBuilderSharedValueType::BaseNormal);
	TriplanarUVsFunctionCall.InArgument("Position", FGraphBuilderOutputPin(UVWInputExpression, 0));
	TriplanarUVsFunctionCall.InArgument("Normal", Normal);
	TriplanarUVsFunctionCall.InArgument("Falloff", FGraphBuilderOutputPin(FalloffInputExpression, 0));

	TriplanarUVsFunctionCall.OutArgument("UVX", ECustomMaterialOutputType::CMOT_Float2);
	TriplanarUVsFunctionCall.OutArgument("UVY", ECustomMaterialOutputType::CMOT_Float2);
	TriplanarUVsFunctionCall.OutArgument("UVZ", ECustomMaterialOutputType::CMOT_Float2);
	TriplanarUVsFunctionCall.OutArgument("Weights", ECustomMaterialOutputType::CMOT_Float3);

	UMaterialExpression* TriplanarUVsFunctionCallExp = TriplanarUVsFunctionCall.Build(Builder);

	// Triplanar Tangents

	HLSLFunctionCallNodeBuilder TriplanarTangentsFunctionCall("TriplanarTangents", "/Plugin/TextureSets/Triplanar.ush");

	TriplanarTangentsFunctionCall.InArgument("Normal", Builder->GetSharedValue(FSubSampleAddress::Root(), EGraphBuilderSharedValueType::BaseNormal));

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
			SubsampleDefinitions.Add(FSubSampleDefinition("X", FGraphBuilderOutputPin(XMask, 0)));

			UMaterialExpressionComponentMask* YMask = Builder->CreateExpression<UMaterialExpressionComponentMask>();
			YMask->R = false;
			YMask->G = true;
			YMask->B = false;
			YMask->A = false;
			Builder->Connect(TriplanarUVsFunctionCallExp, 4, YMask, 0);
			SubsampleDefinitions.Add(FSubSampleDefinition("Y", FGraphBuilderOutputPin(YMask, 0)));

			UMaterialExpressionComponentMask* ZMask = Builder->CreateExpression<UMaterialExpressionComponentMask>();
			ZMask->R = false;
			ZMask->G = false;
			ZMask->B = true;
			ZMask->A = false;
			Builder->Connect(TriplanarUVsFunctionCallExp, 4, ZMask, 0);
			SubsampleDefinitions.Add(FSubSampleDefinition("Z", FGraphBuilderOutputPin(ZMask, 0)));

			TArray<SubSampleHandle> SubsampleHandles = Builder->AddSubsampleGroup(SubsampleDefinitions);

			Builder->AddSubsampleFunction(ConfigureSubsampleFunction([this, SubsampleHandles, TriplanarUVsFunctionCallExp, TriplanarTangentsFunctionCallExp, Builder](FTextureSetSubsampleBuilder& Subsample)
			{
				int32 RelevantIndex = SubsampleHandles.IndexOfByPredicate([&](const FGuid& guid) { return Subsample.IsRelevant(guid); });

				Subsample.SetSharedValue(EGraphBuilderSharedValueType::Texcoord_Raw, FGraphBuilderOutputPin(TriplanarUVsFunctionCallExp, RelevantIndex + 1));
				Subsample.SetSharedValue(EGraphBuilderSharedValueType::Tangent, FGraphBuilderOutputPin(TriplanarTangentsFunctionCallExp, RelevantIndex * 2 + 1));
				Subsample.SetSharedValue(EGraphBuilderSharedValueType::Bitangent, FGraphBuilderOutputPin(TriplanarTangentsFunctionCallExp, RelevantIndex * 2 + 2));
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

			SingleSampleTriplanarFunctionCall.InArgument("UVX", FGraphBuilderOutputPin(TriplanarUVsFunctionCallExp, 1));
			SingleSampleTriplanarFunctionCall.InArgument("UVY", FGraphBuilderOutputPin(TriplanarUVsFunctionCallExp, 2));
			SingleSampleTriplanarFunctionCall.InArgument("UVZ", FGraphBuilderOutputPin(TriplanarUVsFunctionCallExp, 3));

			SingleSampleTriplanarFunctionCall.InArgument("TangentX", FGraphBuilderOutputPin(TriplanarTangentsFunctionCallExp, 1));
			SingleSampleTriplanarFunctionCall.InArgument("BitangentX", FGraphBuilderOutputPin(TriplanarTangentsFunctionCallExp, 2));
			SingleSampleTriplanarFunctionCall.InArgument("TangentY", FGraphBuilderOutputPin(TriplanarTangentsFunctionCallExp, 3));
			SingleSampleTriplanarFunctionCall.InArgument("BitangentY", FGraphBuilderOutputPin(TriplanarTangentsFunctionCallExp, 4));
			SingleSampleTriplanarFunctionCall.InArgument("TangentZ", FGraphBuilderOutputPin(TriplanarTangentsFunctionCallExp, 5));
			SingleSampleTriplanarFunctionCall.InArgument("BitangentZ", FGraphBuilderOutputPin(TriplanarTangentsFunctionCallExp, 6));

			SingleSampleTriplanarFunctionCall.InArgument("Weights", FGraphBuilderOutputPin(TriplanarUVsFunctionCallExp, 4));
			SingleSampleTriplanarFunctionCall.InArgument("Noise", FGraphBuilderOutputPin(NoiseFunctionCallExp, 1));

			SingleSampleTriplanarFunctionCall.OutArgument("UVs", ECustomMaterialOutputType::CMOT_Float2);
			SingleSampleTriplanarFunctionCall.OutArgument("Tangent", ECustomMaterialOutputType::CMOT_Float3);
			SingleSampleTriplanarFunctionCall.OutArgument("Bitangent", ECustomMaterialOutputType::CMOT_Float3);
			SingleSampleTriplanarFunctionCall.OutArgument("DDX", ECustomMaterialOutputType::CMOT_Float2);
			SingleSampleTriplanarFunctionCall.OutArgument("DDY", ECustomMaterialOutputType::CMOT_Float2);

			UMaterialExpression* SingleSampleTriplanarFunctionCallExp = SingleSampleTriplanarFunctionCall.Build(Builder);

			Builder->AddSubsampleFunction(ConfigureSubsampleFunction([this, SampleParams, SingleSampleTriplanarFunctionCallExp, Builder](FTextureSetSubsampleBuilder& Subsample)
			{
				Subsample.SetSharedValue(EGraphBuilderSharedValueType::Texcoord_Raw, FGraphBuilderOutputPin(SingleSampleTriplanarFunctionCallExp, 1));
				Subsample.SetSharedValue(EGraphBuilderSharedValueType::Tangent, FGraphBuilderOutputPin(SingleSampleTriplanarFunctionCallExp, 2));
				Subsample.SetSharedValue(EGraphBuilderSharedValueType::Bitangent, FGraphBuilderOutputPin(SingleSampleTriplanarFunctionCallExp, 3));
				Subsample.SetSharedValue(EGraphBuilderSharedValueType::Texcoord_DDX, FGraphBuilderOutputPin(SingleSampleTriplanarFunctionCallExp, 4));
				Subsample.SetSharedValue(EGraphBuilderSharedValueType::Texcoord_DDY, FGraphBuilderOutputPin(SingleSampleTriplanarFunctionCallExp, 5));
			}));
			break;
		}
	}
}
