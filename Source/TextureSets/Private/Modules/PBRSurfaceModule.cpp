// (c) Electronic Arts. All Rights Reserved.

#include "Modules/PBRSurfaceModule.h"


TArray<TextureSetTextureDef> UPBRSurfaceModule::GetSourceTextures() const
{
	TArray<TextureSetTextureDef> SourceMaps;

	switch (SourceParamaterization)
	{
	case EPBRParamaterization::Basecolor_Metal:
		SourceMaps.Add(TextureSetTextureDef{MetalName, false, 1, FVector4(0, 0, 0, 0)});
		// Falls through
	case EPBRParamaterization::Basecolor:
		SourceMaps.Add(TextureSetTextureDef{BaseColorName, true, 3, FVector4(0.5, 0.5, 0.5, 0)});
		break;
	case EPBRParamaterization::Albedo_Spec:
		SourceMaps.Add(TextureSetTextureDef{AlbedoName, true, 3, FVector4(0.5, 0.5, 0.5, 0)});
		SourceMaps.Add(TextureSetTextureDef{SpecularName, true, 3, FVector4(0, 0, 0, 0)});
		break;
	case EPBRParamaterization::Albedo_F0_F90:
		SourceMaps.Add(TextureSetTextureDef{AlbedoName, true, 3, FVector4(0.5, 0.5, 0.5, 0)});
		SourceMaps.Add(TextureSetTextureDef{F0Name, true, 3, FVector4(0, 0, 0, 0) });
		SourceMaps.Add(TextureSetTextureDef{F90Name, true, 3, FVector4(1, 1, 1, 0) });
		break;
	default:
		break;
	}

	switch (SourceMicrosurface)
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

	switch (SourceNormal)
	{
	case EPBRNormal::Tangent:
		SourceMaps.Add(TextureSetTextureDef{TangentNormalName, false, 3, FVector4(0.5, 0.5, 1, 0)});
		break;
	default:
		break;
	}

	return SourceMaps;
};

// Output pins on the sampler node
TArray<OutputElementDef> UPBRSurfaceModule::GetOutputElements(const UTextureSetSampleParams* SampleParams) const
{
	const UPBRSampleParams* PBRSampleParams = Cast<UPBRSampleParams>(SampleParams);
	
	if (!IsValid(PBRSampleParams)) // Use default
		PBRSampleParams = GetDefault<UPBRSampleParams>();

	TArray<OutputElementDef> OutputElements;

	switch (PBRSampleParams->ParameterizationOutput)
	{
	case EPBRParamaterization::Basecolor_Metal:
		OutputElements.Add(OutputElementDef{MetalName, EMaterialValueType::MCT_LWCScalar});
		// Falls through
	case EPBRParamaterization::Basecolor:
		OutputElements.Add(OutputElementDef{BaseColorName, EMaterialValueType::MCT_Float3});
		break;
	case EPBRParamaterization::Albedo_Spec:
		OutputElements.Add(OutputElementDef{AlbedoName, EMaterialValueType::MCT_Float3});
		OutputElements.Add(OutputElementDef{SpecularName, EMaterialValueType::MCT_Float3});
		break;
	case EPBRParamaterization::Albedo_F0_F90:
		OutputElements.Add(OutputElementDef{AlbedoName, EMaterialValueType::MCT_Float3});
		OutputElements.Add(OutputElementDef{F0Name, EMaterialValueType::MCT_Float3});
		OutputElements.Add(OutputElementDef{F90Name, EMaterialValueType::MCT_Float3});
		break;
	default:
		break;
	}

	switch (PBRSampleParams->MicrosurfaceOutput)
	{
	case EPBRMicrosurface::Roughness:
		OutputElements.Add(OutputElementDef{RoughnessName, EMaterialValueType::MCT_LWCScalar});
		break;
	case EPBRMicrosurface::Smoothness:
		OutputElements.Add(OutputElementDef{SmoothnessName, EMaterialValueType::MCT_LWCScalar});
		break;
	default:
		break;
	}

	switch (PBRSampleParams->NormalSpaceOutput)
	{
	case EPBRNormalSpace::Tangent:
		OutputElements.Add(OutputElementDef{TangentNormalName, EMaterialValueType::MCT_LWCScalar});
		break;
	case EPBRNormalSpace::World:
		OutputElements.Add(OutputElementDef{WorldNormalName, EMaterialValueType::MCT_LWCScalar});
		break;
	case EPBRNormalSpace::SurfaceGradient:
		OutputElements.Add(OutputElementDef{SurfaceGradientName, EMaterialValueType::MCT_LWCScalar});
		break;
	default:
		break;
	}

	return OutputElements;
}
