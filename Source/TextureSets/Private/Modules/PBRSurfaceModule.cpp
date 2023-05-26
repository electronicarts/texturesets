// (c) Electronic Arts. All Rights Reserved.

#include "Modules/PBRSurfaceModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionDeriveNormalZ.h"
#include "Materials/MaterialExpressionAppendVector.h"

#include "MaterialEditingLibrary.h"

TArray<TextureSetTextureDef> UPBRSurfaceModule::GetSourceTextures() const
{
	TArray<TextureSetTextureDef> SourceMaps;

	switch (Paramaterization)
	{
	case EPBRParamaterization::Basecolor_Metal:
		SourceMaps.Add(TextureSetTextureDef{MetalName, false, 1, FVector4(0, 0, 0, 0)});
		// Falls through
	case EPBRParamaterization::Dielectric:
		SourceMaps.Add(TextureSetTextureDef{BaseColorName, true, 3, FVector4(0.5, 0.5, 0.5, 0)});
		break;
	case EPBRParamaterization::Albedo_Spec:
		SourceMaps.Add(TextureSetTextureDef{AlbedoName, true, 3, FVector4(0.5, 0.5, 0.5, 0)});
		SourceMaps.Add(TextureSetTextureDef{SpecularName, true, 3, FVector4(0, 0, 0, 0)});
		break;
	default:
		break;
	}

	switch (Microsurface)
	{
	case EPBRMicrosurface::Roughness:
		SourceMaps.Add(TextureSetTextureDef{RoughnessName, false, 1, FVector4(0.5, 0.5, 0.5, 0)});
		break;
	case EPBRMicrosurface::Smoothness:
		SourceMaps.Add(TextureSetTextureDef{SmoothnessName, false, 1, FVector4(0.5, 0.5, 0.5, 0)});
		break;
	default:
		break;
	}

	switch (Normal)
	{
	case EPBRNormal::Tangent:
		SourceMaps.Add(TextureSetTextureDef{TangentNormalName, false, 2, FVector4(0.5, 0.5, 1, 0)});
		break;
	default:
		break;
	}

	return SourceMaps;
};

void UPBRSurfaceModule::CollectSampleOutputs(TMap<FName, EMaterialValueType>& Results, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UPBRSampleParams* PBRSampleParams = SampleExpression->GetSampleParams<UPBRSampleParams>();

	switch (PBRSampleParams->ParameterizationOutput)
	{
	case EPBRParamaterization::Basecolor_Metal:
		Results.Add(MetalName, EMaterialValueType::MCT_Float1);
		// Falls through
	case EPBRParamaterization::Dielectric:
		Results.Add(BaseColorName, EMaterialValueType::MCT_Float3);
		break;
	case EPBRParamaterization::Albedo_Spec:
		Results.Add(AlbedoName, EMaterialValueType::MCT_Float3);
		Results.Add(SpecularName, EMaterialValueType::MCT_Float3);
		break;
	default:
		break;
	}

	switch (PBRSampleParams->MicrosurfaceOutput)
	{
	case EPBRMicrosurface::Roughness:
		Results.Add(RoughnessName, EMaterialValueType::MCT_Float1);
		break;
	case EPBRMicrosurface::Smoothness:
		Results.Add(SmoothnessName, EMaterialValueType::MCT_Float1);
		break;
	default:
		break;
	}

	switch (PBRSampleParams->NormalSpaceOutput)
	{
	case EPBRNormalSpace::Tangent:
		Results.Add(TangentNormalName, EMaterialValueType::MCT_Float3);
		break;
	case EPBRNormalSpace::World:
		Results.Add(WorldNormalName, EMaterialValueType::MCT_Float3);
		break;
	case EPBRNormalSpace::SurfaceGradient:
		Results.Add(SurfaceGradientName, EMaterialValueType::MCT_Float3);
		break;
	default:
		break;
	}
}

void UPBRSurfaceModule::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	const UPBRSampleParams* PBRSampleParams = SampleExpression->GetSampleParams<UPBRSampleParams>();

	// Surface Parameterization
	{
		if (Paramaterization == PBRSampleParams->ParameterizationOutput) // Easy, we just output values directly
		{
			switch (Paramaterization)
			{
			case EPBRParamaterization::Basecolor_Metal:
				Builder.GetProcessedTextureSample(MetalName)->ConnectExpression(Builder.GetOutput(MetalName)->GetInput(0), 0);
				// Falls through
			case EPBRParamaterization::Dielectric:
				Builder.GetProcessedTextureSample(BaseColorName)->ConnectExpression(Builder.GetOutput(BaseColorName)->GetInput(0), 0);
				break;
			case EPBRParamaterization::Albedo_Spec:
				Builder.GetProcessedTextureSample(AlbedoName)->ConnectExpression(Builder.GetOutput(AlbedoName)->GetInput(0), 0);
				Builder.GetProcessedTextureSample(SpecularName)->ConnectExpression(Builder.GetOutput(SpecularName)->GetInput(0), 0);
				break;
			default:
				break;
			}
		}
	}

	// Microsurface
	{
		TObjectPtr<UMaterialExpression> ProcessedNode = nullptr;
		if (Microsurface == EPBRMicrosurface::Roughness)
			ProcessedNode = Builder.GetProcessedTextureSample(RoughnessName);
		else // EPBRMicrosurface::Smoothness:
			ProcessedNode = Builder.GetProcessedTextureSample(SmoothnessName);

		TObjectPtr<UMaterialExpressionFunctionOutput> ResultNode;
		if (PBRSampleParams->MicrosurfaceOutput == EPBRMicrosurface::Roughness)
			ResultNode = Builder.GetOutput(RoughnessName);
		else // EPBRMicrosurface::Smoothness:
			ResultNode = Builder.GetOutput(SmoothnessName);

		bool bNeedsConversion = Microsurface != PBRSampleParams->MicrosurfaceOutput;

		if (bNeedsConversion)
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
		}
		

		switch (PBRSampleParams->NormalSpaceOutput)
		{
		case EPBRNormalSpace::Tangent:
			TangentNormal->ConnectExpression(Builder.GetOutput(TangentNormalName)->GetInput(0), 0);
			break;
		case EPBRNormalSpace::World:
			// TODO
			break;
		case EPBRNormalSpace::SurfaceGradient:
			// TODO
			break;
		default:
			break;
		}
	}
}