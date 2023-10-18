// (c) Electronic Arts. All Rights Reserved.

#include "Modules/PBRSurfaceModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionDeriveNormalZ.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "ProcessingNodes/TextureOperatorInvert.h"
#endif

#if WITH_EDITOR
void UPBRSurfaceModule::GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	if (Paramaterization == EPBRParamaterization::Basecolor_Metal || Paramaterization == EPBRParamaterization::Dielectric)
	{
		// Base Color
		{
			static const FTextureSetSourceTextureDef BaseColorDef(3, true, FVector4(0.5, 0.5, 0.5, 0));
			Graph.AddOutputTexture(BaseColorName, Graph.AddInputTexture(BaseColorName, BaseColorDef));
		}

		// Metal
		if (Paramaterization == EPBRParamaterization::Basecolor_Metal)
		{
			static const FTextureSetSourceTextureDef MetalDef(1, false, FVector4(0, 0, 0, 0));
			Graph.AddOutputTexture(MetalName, Graph.AddInputTexture(MetalName, MetalDef));
		}
	}
	else if (Paramaterization == EPBRParamaterization::Albedo_Spec)
	{
		// Albedo
		static const FTextureSetSourceTextureDef AlbedoDef(3, true, FVector4(0.5, 0.5, 0.5, 0));
		Graph.AddOutputTexture(AlbedoName, Graph.AddInputTexture(AlbedoName, AlbedoDef));

		// Spec
		static const FTextureSetSourceTextureDef SpecularDef(3, true, FVector4(0, 0, 0, 0));
		Graph.AddOutputTexture(SpecularName, Graph.AddInputTexture(SpecularName, SpecularDef));
	}
	else
	{
		unimplemented()
	}

	if (Microsurface == EPBRMicrosurface::Roughness)
	{
		// Roughness
		static const FTextureSetSourceTextureDef RoughnessDef (1, false, FVector4(0.5, 0.5, 0.5, 0));
		Graph.AddOutputTexture(RoughnessName, Graph.AddInputTexture(RoughnessName, RoughnessDef));
	}
	else if (Microsurface == EPBRMicrosurface::Smoothness)
	{
		// Smoothness
		static const FTextureSetSourceTextureDef SmoothnessDef (1, false, FVector4(0.5, 0.5, 0.5, 0));
		Graph.AddOutputTexture(SmoothnessName, Graph.AddInputTexture(SmoothnessName, SmoothnessDef));
	}
	else
	{
		unimplemented()
	}

	if (Normal == EPBRNormal::Tangent)
	{
		static const FTextureSetSourceTextureDef TangentNormalDef (2, false, FVector4(0.5, 0.5, 1, 0));
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
void UPBRSurfaceModule::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	const UPBRSampleParams* PBRSampleParams = SampleExpression->SampleParams.Get<UPBRSampleParams>();

	// Surface Parameterization
	{
		switch (Paramaterization)
		{
		case EPBRParamaterization::Basecolor_Metal:
			Builder.Connect(Builder.GetProcessedTextureSample(MetalName), Builder.CreateOutput(MetalName), 0);
			// Falls through
		case EPBRParamaterization::Dielectric:
			Builder.Connect(Builder.GetProcessedTextureSample(BaseColorName), Builder.CreateOutput(BaseColorName), 0);
			break;
		case EPBRParamaterization::Albedo_Spec:
			Builder.Connect(Builder.GetProcessedTextureSample(AlbedoName), Builder.CreateOutput(AlbedoName), 0);
			Builder.Connect(Builder.GetProcessedTextureSample(SpecularName), Builder.CreateOutput(SpecularName), 0);
			break;
		default:
			unimplemented()
			break;
		}
	}

	// Microsurface
	{
		FGraphBuilderOutputAddress MicrosurfaceSample;
		if (Microsurface == EPBRMicrosurface::Roughness)
			MicrosurfaceSample = Builder.GetProcessedTextureSample(RoughnessName);
		else if (Microsurface == EPBRMicrosurface::Smoothness)
			MicrosurfaceSample = Builder.GetProcessedTextureSample(SmoothnessName);
		else
			unimplemented()

		TObjectPtr<UMaterialExpressionFunctionOutput> ResultNode;
		if (PBRSampleParams->MicrosurfaceOutput == EPBRMicrosurface::Roughness)
			ResultNode = Builder.CreateOutput(RoughnessName);
		else if (PBRSampleParams->MicrosurfaceOutput == EPBRMicrosurface::Smoothness)
			ResultNode = Builder.CreateOutput(SmoothnessName);
		else
			unimplemented()

		bool bNeedsInversion = Microsurface != PBRSampleParams->MicrosurfaceOutput;

		if (bNeedsInversion)
		{
			UMaterialExpressionOneMinus* OneMinus = Builder.CreateExpression<UMaterialExpressionOneMinus>();
			Builder.Connect(MicrosurfaceSample, OneMinus, 0);
			Builder.Connect(OneMinus, 0, ResultNode, 0);
		}
		else
		{
			Builder.Connect(MicrosurfaceSample, ResultNode, 0);
		}
	}

	// Normals
	if (Normal == EPBRNormal::Tangent)
	{
		FGraphBuilderOutputAddress TangentNormal = Builder.GetProcessedTextureSample(TangentNormalName);

		// Unpack tangent normal X and Y
		UMaterialExpressionMultiply* Mul = Builder.CreateExpression<UMaterialExpressionMultiply>();
		Builder.Connect(TangentNormal, Mul, 0);
		Mul->ConstB = 2.0f;

		UMaterialExpressionAdd* Add = Builder.CreateExpression<UMaterialExpressionAdd>();
		Builder.Connect(Mul, 0, Add, 0);
		Add->ConstB = -1.0f;

		// Derive Z value
		UMaterialExpressionDeriveNormalZ* DeriveZ = Builder.CreateExpression<UMaterialExpressionDeriveNormalZ>();
		Builder.Connect(Add, 0, DeriveZ, 0);

		if (PBRSampleParams->NormalOutput == EPBRNormalSpace::Tangent)
		{
			// Output tangent normal
			Builder.Connect(DeriveZ, 0, Builder.CreateOutput(TangentNormalName), 0);
		}
		else
		{
			UMaterialExpression* Transform3x3 = Builder.CreateFunctionCall(FSoftObjectPath(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Math/Transform3x3Matrix.Transform3x3Matrix")));
			Builder.Connect(DeriveZ, 0, Transform3x3, "VectorToTransform");
			Builder.Connect(Builder.GetSharedValue(EGraphBuilderSharedValueType::Tangent), Transform3x3, "BasisX");
			Builder.Connect(Builder.GetSharedValue(EGraphBuilderSharedValueType::Bitangent), Transform3x3, "BasisY");
			Builder.Connect(Builder.GetSharedValue(EGraphBuilderSharedValueType::BaseNormal), Transform3x3, "BasisZ");

			if (PBRSampleParams->NormalOutput == EPBRNormalSpace::World)
			{
				// Output world normal
				Builder.Connect(Transform3x3, 0, Builder.CreateOutput(WorldNormalName), 0);
			}
			else if (PBRSampleParams->NormalOutput == EPBRNormalSpace::SurfaceGradient)
			{
				UMaterialExpressionSubtract* Subtract = Builder.CreateExpression<UMaterialExpressionSubtract>();
				Builder.Connect(Transform3x3, 0, Subtract, 0);
				Builder.Connect(Builder.GetSharedValue(EGraphBuilderSharedValueType::BaseNormal), Subtract, 1);

				// Output surface gradient
				Builder.Connect(Subtract, 0, Builder.CreateOutput(SurfaceGradientName), 0);
			}
			else
			{
				unimplemented();
			}
		}
	}
}
#endif
