// (c) Electronic Arts. All Rights Reserved.

#include "Modules/PBRSurfaceModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionDeriveNormalZ.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "ProcessingNodes/TextureInput.h"
#include "ProcessingNodes/TextureOperatorInvert.h"
#endif

#if WITH_EDITOR
void UPBRSurfaceModule::ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	if (Paramaterization == EPBRParamaterization::Basecolor_Metal || Paramaterization == EPBRParamaterization::Dielectric)
	{
		// Base Color
		{
			static const FTextureSetSourceTextureDef BaseColorDef(3, ETextureSetChannelEncoding::SRGB, FVector4(0.5, 0.5, 0.5, 0));
			Graph.AddOutputTexture(BaseColorName, Graph.AddInputTexture(BaseColorName, BaseColorDef));
		}

		// Metal
		if (Paramaterization == EPBRParamaterization::Basecolor_Metal)
		{
			static const FTextureSetSourceTextureDef MetalDef(1, ETextureSetChannelEncoding::None, FVector4(0, 0, 0, 0));
			Graph.AddOutputTexture(MetalName, Graph.AddInputTexture(MetalName, MetalDef));
		}
	}
	else if (Paramaterization == EPBRParamaterization::Albedo_Spec)
	{
		// Albedo
		static const FTextureSetSourceTextureDef AlbedoDef(3, ETextureSetChannelEncoding::SRGB, FVector4(0.5, 0.5, 0.5, 0));
		Graph.AddOutputTexture(AlbedoName, Graph.AddInputTexture(AlbedoName, AlbedoDef));

		// Spec
		static const FTextureSetSourceTextureDef SpecularDef(3, ETextureSetChannelEncoding::SRGB, FVector4(0, 0, 0, 0));
		Graph.AddOutputTexture(SpecularName, Graph.AddInputTexture(SpecularName, SpecularDef));
	}
	else
	{
		unimplemented()
	}

	if (Microsurface == EPBRMicrosurface::Roughness)
	{
		// Roughness
		static const FTextureSetSourceTextureDef RoughnessDef (1, ETextureSetChannelEncoding::RangeCompression, FVector4(0.5, 0.5, 0.5, 0));
		Graph.AddOutputTexture(RoughnessName, Graph.AddInputTexture(RoughnessName, RoughnessDef));
	}
	else if (Microsurface == EPBRMicrosurface::Smoothness)
	{
		// Smoothness
		static const FTextureSetSourceTextureDef SmoothnessDef (1, ETextureSetChannelEncoding::RangeCompression, FVector4(0.5, 0.5, 0.5, 0));
		Graph.AddOutputTexture(SmoothnessName, Graph.AddInputTexture(SmoothnessName, SmoothnessDef));
	}
	else
	{
		unimplemented()
	}

	if (Normal == EPBRNormal::Tangent)
	{
		static const FTextureSetSourceTextureDef TangentNormalDef (2, ETextureSetChannelEncoding::RangeCompression, FVector4(0.5, 0.5, 1, 0));
		Graph.AddOutputTexture(TangentNormalName, Graph.AddInputTexture(TangentNormalName, TangentNormalDef));
	}
	else if (Normal != EPBRNormal::None)
	{
		unimplemented();
	}
}
#endif

#if WITH_EDITOR
int32 UPBRSurfaceModule::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UPBRSampleParams* SampleParams = SampleExpression->SampleParams.Get<UPBRSampleParams>();

	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);

	Hash = HashCombine(Hash, GetTypeHash(SampleParams->MicrosurfaceOutput));
	Hash = HashCombine(Hash, GetTypeHash(SampleParams->NormalOutput));
	Hash = HashCombine(Hash, GetTypeHash(Paramaterization));
	Hash = HashCombine(Hash, GetTypeHash(Microsurface));
	Hash = HashCombine(Hash, GetTypeHash(Normal));

	return Hash;
}
#endif

#if WITH_EDITOR
void UPBRSurfaceModule::ConfigureSamplingGraphBuilder(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder* Builder) const
{
	const UPBRSampleParams* PBRSampleParams = SampleExpression->SampleParams.Get<UPBRSampleParams>();

	Builder->AddSampleBuilder(SampleBuilderFunction([this, Builder, PBRSampleParams](FTextureSetSubsampleContext& SampleContext)
	{
		// Surface Parameterization
		{
			switch (Paramaterization)
			{
			case EPBRParamaterization::Basecolor_Metal:
				SampleContext.AddResult(MetalName, SampleContext.GetProcessedTextureSample(MetalName));
				// Falls through
			case EPBRParamaterization::Dielectric:
				SampleContext.AddResult(BaseColorName, SampleContext.GetProcessedTextureSample(BaseColorName));
				break;
			case EPBRParamaterization::Albedo_Spec:
				SampleContext.AddResult(AlbedoName, SampleContext.GetProcessedTextureSample(AlbedoName));
				SampleContext.AddResult(SpecularName, SampleContext.GetProcessedTextureSample(SpecularName));
				break;
			default:
				unimplemented()
				break;
			}
		}

		// Microsurface
		if (Microsurface != EPBRMicrosurface::None && PBRSampleParams->MicrosurfaceOutput != EPBRMicrosurface::None)
		{
			FGraphBuilderOutputAddress MicrosurfaceSample;
			if (Microsurface == EPBRMicrosurface::Roughness)
				MicrosurfaceSample = SampleContext.GetProcessedTextureSample(RoughnessName);
			else if (Microsurface == EPBRMicrosurface::Smoothness)
				MicrosurfaceSample = SampleContext.GetProcessedTextureSample(SmoothnessName);
			else
				unimplemented()

			FGraphBuilderOutputAddress ResultAddress = MicrosurfaceSample;

			if (Microsurface != PBRSampleParams->MicrosurfaceOutput)
			{
				UMaterialExpressionOneMinus* OneMinus = Builder->CreateExpression<UMaterialExpressionOneMinus>();
				Builder->Connect(ResultAddress, OneMinus, 0);
				ResultAddress = FGraphBuilderOutputAddress(OneMinus, 0);
			}

			FName ResultName;

			if (PBRSampleParams->MicrosurfaceOutput == EPBRMicrosurface::Roughness)
				ResultName = RoughnessName;
			else if (PBRSampleParams->MicrosurfaceOutput == EPBRMicrosurface::Smoothness)
				ResultName = SmoothnessName;
			else
				unimplemented()

			SampleContext.AddResult(ResultName, ResultAddress);
		}

		// Normals
		if (Normal == EPBRNormal::Tangent)
		{
			FGraphBuilderOutputAddress TangentNormal = SampleContext.GetProcessedTextureSample(TangentNormalName);

			// Unpack tangent normal X and Y
			UMaterialExpressionMultiply* Mul = Builder->CreateExpression<UMaterialExpressionMultiply>();
			Builder->Connect(TangentNormal, Mul, 0);
			Mul->ConstB = 2.0f;

			UMaterialExpressionAdd* Add = Builder->CreateExpression<UMaterialExpressionAdd>();
			Builder->Connect(Mul, 0, Add, 0);
			Add->ConstB = -1.0f;

			// Derive Z value
			UMaterialExpressionDeriveNormalZ* DeriveZ = Builder->CreateExpression<UMaterialExpressionDeriveNormalZ>();
			Builder->Connect(Add, 0, DeriveZ, 0);

			if (PBRSampleParams->NormalOutput == EPBRNormalSpace::Tangent)
			{
				// Output tangent normal
				SampleContext.AddResult(TangentNormalName, FGraphBuilderOutputAddress(DeriveZ, 0));
			}
			else
			{
				UMaterialExpression* Transform3x3 = Builder->CreateFunctionCall(FSoftObjectPath(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Math/Transform3x3Matrix.Transform3x3Matrix")));
				Builder->Connect(DeriveZ, 0, Transform3x3, "VectorToTransform");
				Builder->Connect(SampleContext.GetSharedValue(EGraphBuilderSharedValueType::Tangent), Transform3x3, "BasisX");
				Builder->Connect(SampleContext.GetSharedValue(EGraphBuilderSharedValueType::Bitangent), Transform3x3, "BasisY");
				Builder->Connect(SampleContext.GetSharedValue(EGraphBuilderSharedValueType::BaseNormal), Transform3x3, "BasisZ");

				if (PBRSampleParams->NormalOutput == EPBRNormalSpace::World)
				{
					// Output world normal
					SampleContext.AddResult(WorldNormalName, FGraphBuilderOutputAddress(Transform3x3, 0));
				}
				else if (PBRSampleParams->NormalOutput == EPBRNormalSpace::SurfaceGradient)
				{
					UMaterialExpressionSubtract* Subtract = Builder->CreateExpression<UMaterialExpressionSubtract>();
					Builder->Connect(Transform3x3, 0, Subtract, 0);
					Builder->Connect(SampleContext.GetSharedValue(EGraphBuilderSharedValueType::BaseNormal), Subtract, 1);

					// Output surface gradient
					SampleContext.AddResult(SurfaceGradientName, FGraphBuilderOutputAddress(Subtract, 0));
				}
				else
				{
					unimplemented();
				}
			}
		}
	}));
}
#endif
