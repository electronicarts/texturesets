// (c) Electronic Arts. All Rights Reserved.

#include "Modules/PBRSurfaceModule.h"
#include "MaterialExpressionTextureSetSampleParameter.h"

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

// Output pins on the sampler node
void UPBRSurfaceModule::CollectSampleOutputs(TMap<FName, EMaterialValueType>& Results, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UPBRSampleParams* PBRSampleParams = SampleExpression->GetSampleParams<UPBRSampleParams>();

	switch (PBRSampleParams->ParameterizationOutput)
	{
	case EPBRParamaterization::Basecolor_Metal:
		Results.Add(MetalName, EMaterialValueType::MCT_Float);
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
		Results.Add(RoughnessName, EMaterialValueType::MCT_Float);
		break;
	case EPBRMicrosurface::Smoothness:
		Results.Add(SmoothnessName, EMaterialValueType::MCT_Float);
		break;
	default:
		break;
	}

	switch (PBRSampleParams->NormalSpaceOutput)
	{
	case EPBRNormalSpace::Tangent:
		Results.Add(TangentNormalName, EMaterialValueType::MCT_Float);
		break;
	case EPBRNormalSpace::World:
		Results.Add(WorldNormalName, EMaterialValueType::MCT_Float);
		break;
	case EPBRNormalSpace::SurfaceGradient:
		Results.Add(SurfaceGradientName, EMaterialValueType::MCT_Float);
		break;
	default:
		break;
	}
}
