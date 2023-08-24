// (c) Electronic Arts. All Rights Reserved.

#include "Modules/PBRSurfaceModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"

#if WITH_EDITOR
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionDeriveNormalZ.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "MaterialEditingLibrary.h"
#include "ProcessingNodes/TextureOperatorInvert.h"
#endif

#if WITH_EDITOR
void UPBRSurfaceModule::GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	if (Paramaterization == EPBRParamaterization::Basecolor_Metal || Paramaterization == EPBRParamaterization::Dielectric)
	{
		// Base Color
		{
			static const FTextureSetSourceTextureDef BaseColorDef {true, 3, FVector4(0.5, 0.5, 0.5, 0)};
			Graph.AddOutputTexture(BaseColorName, Graph.AddInputTexture(BaseColorName, BaseColorDef));
		}

		// Metal
		if (Paramaterization == EPBRParamaterization::Basecolor_Metal)
		{
			static const FTextureSetSourceTextureDef MetalDef {false, 1, FVector4(0, 0, 0, 0)};
			Graph.AddOutputTexture(MetalName, Graph.AddInputTexture(MetalName, MetalDef));
		}
	}
	else if (Paramaterization == EPBRParamaterization::Albedo_Spec)
	{
		// Albedo
		static const FTextureSetSourceTextureDef AlbedoDef {true, 3, FVector4(0.5, 0.5, 0.5, 0)};
		Graph.AddOutputTexture(AlbedoName, Graph.AddInputTexture(AlbedoName, AlbedoDef));

		// Spec
		static const FTextureSetSourceTextureDef SpecularDef {true, 3, FVector4(0, 0, 0, 0)};
		Graph.AddOutputTexture(SpecularName, Graph.AddInputTexture(SpecularName, SpecularDef));
	}
	else
	{
		unimplemented()
	}

	if (Microsurface == EPBRMicrosurface::Roughness)
	{
		// Roughness
		static const FTextureSetSourceTextureDef RoughnessDef {false, 1, FVector4(0.5, 0.5, 0.5, 0)};
		Graph.AddOutputTexture(RoughnessName, Graph.AddInputTexture(RoughnessName, RoughnessDef));
	}
	else if (Microsurface == EPBRMicrosurface::Smoothness)
	{
		// Smoothness
		static const FTextureSetSourceTextureDef SmoothnessDef {false, 1, FVector4(0.5, 0.5, 0.5, 0)};
		Graph.AddOutputTexture(SmoothnessName, Graph.AddInputTexture(SmoothnessName, SmoothnessDef));
	}
	else
	{
		unimplemented()
	}

	if (Normal != EPBRNormal::None)
	{
		if (Normal == EPBRNormal::Tangent)
		{
			static const FTextureSetSourceTextureDef TangentNormalDef {false, 2, FVector4(0.5, 0.5, 1, 0)};
			Graph.AddOutputTexture(TangentNormalName, Graph.AddInputTexture(TangentNormalName, TangentNormalDef));
		}
		else
		{
			unimplemented()
		}
	}
}
#endif

#if WITH_EDITOR
int32 UPBRSurfaceModule::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UPBRSampleParams* SampleParams = SampleExpression->GetSampleParams<UPBRSampleParams>();

	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);

	//Hash = HashCombine(Hash, GetTypeHash(SampleParams->ParameterizationOutput));
	//Hash = HashCombine(Hash, GetTypeHash(SampleParams->NormalSpaceOutput));
	Hash = HashCombine(Hash, GetTypeHash(SampleParams->MicrosurfaceOutput));
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
	const UPBRSampleParams* PBRSampleParams = SampleExpression->GetSampleParams<UPBRSampleParams>();

	// Surface Parameterization
	{
		switch (Paramaterization)
		{
		case EPBRParamaterization::Basecolor_Metal:
			Builder.GetProcessedTextureSample(MetalName)->ConnectExpression(Builder.CreateOutput(MetalName)->GetInput(0), 0);
			// Falls through
		case EPBRParamaterization::Dielectric:
			Builder.GetProcessedTextureSample(BaseColorName)->ConnectExpression(Builder.CreateOutput(BaseColorName)->GetInput(0), 0);
			break;
		case EPBRParamaterization::Albedo_Spec:
			Builder.GetProcessedTextureSample(AlbedoName)->ConnectExpression(Builder.CreateOutput(AlbedoName)->GetInput(0), 0);
			Builder.GetProcessedTextureSample(SpecularName)->ConnectExpression(Builder.CreateOutput(SpecularName)->GetInput(0), 0);
			break;
		default:
			unimplemented()
			break;
		}
	}

	// Microsurface
	{
		TObjectPtr<UMaterialExpression> ProcessedNode = nullptr;
		if (Microsurface == EPBRMicrosurface::Roughness)
			ProcessedNode = Builder.GetProcessedTextureSample(RoughnessName);
		else if (Microsurface == EPBRMicrosurface::Smoothness)
			ProcessedNode = Builder.GetProcessedTextureSample(SmoothnessName);
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
			ProcessedNode->ConnectExpression(OneMinus->GetInput(0), 0);
			OneMinus->ConnectExpression(ResultNode->GetInput(0), 0);
		}
		else
		{
			ProcessedNode->ConnectExpression(ResultNode->GetInput(0), 0);
		}
	}

	// Normals
	if (Normal != EPBRNormal::None)
	{
		UMaterialExpression* TangentNormal = Builder.GetProcessedTextureSample(TangentNormalName);

		if (Normal == EPBRNormal::Tangent)
		{
			// Unpack tangent normal X and Y
			UMaterialExpressionMultiply* Mul = Builder.CreateExpression<UMaterialExpressionMultiply>();
			TangentNormal->ConnectExpression(Mul->GetInput(0), 0);
			Mul->ConstB = 2.0f;

			UMaterialExpressionAdd* Add = Builder.CreateExpression<UMaterialExpressionAdd>();
			Mul->ConnectExpression(&Add->A, 0);
			Add->ConstB = -1.0f;

			UMaterialExpressionDeriveNormalZ* DeriveZ = Builder.CreateExpression<UMaterialExpressionDeriveNormalZ>();
			Add->ConnectExpression(DeriveZ->GetInput(0), 0);

			// Now use this as our unpacked tangent normal
			TangentNormal = DeriveZ;

			TangentNormal->ConnectExpression(Builder.CreateOutput(TangentNormalName)->GetInput(0), 0);
		}
		else
			unimplemented();
	}
}
#endif
