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
#include "Textures/TextureOperatorInvert.h"
#endif

void UPBRSurfaceModule::BuildSharedInfo(TextureSetDefinitionSharedInfo& Info) const
{
	static const TextureSetTextureDef MetalDef {MetalName, false, 1, FVector4(0, 0, 0, 0)};
	static const TextureSetTextureDef BaseColorDef {BaseColorName, true, 3, FVector4(0.5, 0.5, 0.5, 0)};
	static const TextureSetTextureDef AlbedoDef {AlbedoName, true, 3, FVector4(0.5, 0.5, 0.5, 0)};
	static const TextureSetTextureDef SpecularDef {SpecularName, true, 3, FVector4(0, 0, 0, 0)};
	static const TextureSetTextureDef RoughnessDef {RoughnessName, false, 1, FVector4(0.5, 0.5, 0.5, 0)};
	static const TextureSetTextureDef SmoothnessDef {SmoothnessName, false, 1, FVector4(0.5, 0.5, 0.5, 0)};
	static const TextureSetTextureDef TangentNormalDef {TangentNormalName, false, 2, FVector4(0.5, 0.5, 1, 0)};

	switch (Paramaterization)
	{
	case EPBRParamaterization::Basecolor_Metal:
		Info.AddSourceTexture(MetalDef);
		Info.AddProcessedTexture(MetalDef);
		// Falls through
	case EPBRParamaterization::Dielectric:
		Info.AddSourceTexture(BaseColorDef);
		Info.AddProcessedTexture(BaseColorDef);
		break;
	case EPBRParamaterization::Albedo_Spec:
		Info.AddSourceTexture(AlbedoDef);
		Info.AddProcessedTexture(AlbedoDef);
		Info.AddSourceTexture(SpecularDef);
		Info.AddProcessedTexture(SpecularDef);
		break;
	default:
		break;
	}

	switch (Microsurface)
	{
	case EPBRMicrosurface::Roughness:
		Info.AddSourceTexture(RoughnessDef);
		Info.AddProcessedTexture(RoughnessDef);
		break;
	case EPBRMicrosurface::Smoothness:
		Info.AddSourceTexture(SmoothnessDef);
		Info.AddProcessedTexture(SmoothnessDef);
		break;
	default:
		break;
	}

	switch (Normal)
	{
	case EPBRNormal::Tangent:
		Info.AddSourceTexture(TangentNormalDef);
		Info.AddProcessedTexture(TangentNormalDef);
		break;
	default:
		break;
	}
}

void UPBRSurfaceModule::BuildSamplingInfo(TextureSetDefinitionSamplingInfo& SamplingInfo, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UPBRSampleParams* PBRSampleParams = SampleExpression->GetSampleParams<UPBRSampleParams>();

	switch (PBRSampleParams->ParameterizationOutput)
	{
	case EPBRParamaterization::Basecolor_Metal:
		SamplingInfo.AddSampleOutput(MetalName, EMaterialValueType::MCT_Float1);
		// Falls through
	case EPBRParamaterization::Dielectric:
		SamplingInfo.AddSampleOutput(BaseColorName, EMaterialValueType::MCT_Float3);
		break;
	case EPBRParamaterization::Albedo_Spec:
		SamplingInfo.AddSampleOutput(AlbedoName, EMaterialValueType::MCT_Float3);
		SamplingInfo.AddSampleOutput(SpecularName, EMaterialValueType::MCT_Float3);
		break;
	default:
		break;
	}

	switch (PBRSampleParams->MicrosurfaceOutput)
	{
	case EPBRMicrosurface::Roughness:
		SamplingInfo.AddSampleOutput(RoughnessName, EMaterialValueType::MCT_Float1);
		break;
	case EPBRMicrosurface::Smoothness:
		SamplingInfo.AddSampleOutput(SmoothnessName, EMaterialValueType::MCT_Float1);
		break;
	default:
		break;
	}

	switch (PBRSampleParams->NormalSpaceOutput)
	{
	case EPBRNormalSpace::Tangent:
		SamplingInfo.AddSampleOutput(TangentNormalName, EMaterialValueType::MCT_Float3);
		break;
	case EPBRNormalSpace::World:
		SamplingInfo.AddSampleOutput(WorldNormalName, EMaterialValueType::MCT_Float3);
		break;
	case EPBRNormalSpace::SurfaceGradient:
		SamplingInfo.AddSampleOutput(SurfaceGradientName, EMaterialValueType::MCT_Float3);
		break;
	default:
		break;
	}
}

void UPBRSurfaceModule::Process(FTextureSetProcessingContext& Context) const
{
	const bool HasBaseColor = Context.HasSourceTexure(BaseColorName);
	const bool HasMetal = Context.HasSourceTexure(MetalName);
	const bool HasAlbedo = Context.HasSourceTexure(AlbedoName);
	const bool HasSpec = Context.HasSourceTexure(SpecularName);

	if (Paramaterization == EPBRParamaterization::Basecolor_Metal || Paramaterization == EPBRParamaterization::Dielectric)
	{
		if (HasBaseColor)
		{
			Context.AddProcessedTexture(BaseColorName, Context.GetSourceTexture(BaseColorName));

			if (HasMetal && Paramaterization == EPBRParamaterization::Basecolor_Metal)
			{
				Context.AddProcessedTexture(MetalName, Context.GetSourceTexture(MetalName));
			}
		}
		else if (HasAlbedo)
		{
			if (Paramaterization == EPBRParamaterization::Dielectric)
			{
				// Albedo can be used as basecolor for dielectircs
				Context.AddProcessedTexture(BaseColorName, Context.GetSourceTexture(AlbedoName));
			}
			else
			{
				// TODO: conversion from albedo-spec to basecolor-metal
				unimplemented();
			}
		}
	}
	else if (Paramaterization == EPBRParamaterization::Albedo_Spec)
	{
		if (HasAlbedo)
		{
			Context.AddProcessedTexture(AlbedoName, Context.GetSourceTexture(AlbedoName));

			if (HasSpec)
			{
				Context.AddProcessedTexture(SpecularName, Context.GetSourceTexture(SpecularName));
			}
		}
		else if (HasBaseColor)
		{
			if (HasMetal)
			{
				// TODO: conversion from basecolor-metal to albedo-spec
				unimplemented();
			}
			else
			{
				// Basecolor can be used as Albedo if there's no metal
				Context.AddProcessedTexture(AlbedoName, Context.GetSourceTexture(BaseColorName));
			}
		}
	}
	else
	{
		unimplemented();
	}

	if (Microsurface == EPBRMicrosurface::Roughness)
	{
		if (Context.HasSourceTexure(RoughnessName))
		{
			Context.AddProcessedTexture(RoughnessName, Context.GetSourceTexture(RoughnessName));
		}
		else if (Context.HasSourceTexure(SmoothnessName))
		{
			Context.AddProcessedTexture(RoughnessName, MakeShared<FTextureOperatorInvert>(Context.GetSourceTexture(SmoothnessName)));
		}
	}
	else if (Microsurface == EPBRMicrosurface::Smoothness)
	{
		if (Context.HasSourceTexure(SmoothnessName))
		{
			Context.AddProcessedTexture(SmoothnessName, Context.GetSourceTexture(SmoothnessName));
		}
		else if (Context.HasSourceTexure(RoughnessName))
		{
			Context.AddProcessedTexture(SmoothnessName, MakeShared<FTextureOperatorInvert>(Context.GetSourceTexture(RoughnessName)));
		}
	}
	else
	{
		unimplemented();
	}

	if (Normal == EPBRNormal::Tangent)
	{
		Context.AddProcessedTexture(TangentNormalName, Context.GetSourceTexture(TangentNormalName));
	}
}

int32 UPBRSurfaceModule::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UPBRSampleParams* SampleParams = SampleExpression->GetSampleParams<UPBRSampleParams>();

	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);

	Hash = HashCombine(Hash, GetTypeHash(SampleParams->ParameterizationOutput));
	Hash = HashCombine(Hash, GetTypeHash(SampleParams->NormalSpaceOutput));
	Hash = HashCombine(Hash, GetTypeHash(SampleParams->MicrosurfaceOutput));
	Hash = HashCombine(Hash, GetTypeHash(Paramaterization));
	Hash = HashCombine(Hash, GetTypeHash(Microsurface));
	Hash = HashCombine(Hash, GetTypeHash(Normal));

	return Hash;
}

#if WITH_EDITOR
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
#endif
