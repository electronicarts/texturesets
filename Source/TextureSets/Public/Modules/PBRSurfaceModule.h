// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetDefinition.h"
#include "PBRSurfaceModule.generated.h"

UENUM()
enum class EPBRParamaterization
{
	Basecolor_Metal,
	Basecolor, // Purely Dielectric
	Albedo_Spec,
	Albedo_F0_F90,
};

UENUM()
enum class EPBRMicrosurface
{
	Roughness,
	Smoothness,
};

UENUM()
enum class EPBRNormal
{
	None,
	Tangent,
};

UENUM()
enum class EPBRNormalSpace
{
	Tangent,
	World,
	SurfaceGradient,
};

// Class which is instanced on each texture set sampler, and then provided to the module
UCLASS()
class UPBRSampleParams : public UTextureSetSampleParams
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere);
	EPBRParamaterization ParameterizationOutput;

	UPROPERTY(EditAnywhere);
	EPBRNormalSpace NormalSpaceOutput;

	UPROPERTY(EditAnywhere);
	EPBRMicrosurface MicrosurfaceOutput;
};

// PBR material properties, with choices of source and output parameterizations.
UCLASS()
class UPBRSurfaceModule : public UTextureSetDefinitionModule
{
	GENERATED_BODY()
public:

	const FName MetalName = "Metal";
	const FName BaseColorName = "BaseColor";
	const FName AlbedoName = "Albedo";
	const FName SpecularName = "Specular";
	const FName F0Name = "F0";
	const FName F90Name = "F90";
	const FName RoughnessName = "Roughness";
	const FName SmoothnessName = "Smoothness";
	const FName TangentNormalName = "TangentNormal";
	const FName WorldNormalName = "WorldNormal";
	const FName SurfaceGradientName = "SurfaceGradient";

	virtual bool AllowMultiple() override { return false; }

	virtual TArray<TextureSetTextureDef> GetSourceTextures() const override;
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const override { return UPBRSampleParams::StaticClass(); }
	virtual TArray<OutputElementDef> GetOutputElements(const UTextureSetSampleParams* SampleParams) const override;

private:
	UPROPERTY(EditAnywhere)
	EPBRParamaterization SourceParamaterization;

	UPROPERTY(EditAnywhere)
	EPBRMicrosurface SourceMicrosurface;

	UPROPERTY(EditAnywhere)
	EPBRNormal SourceNormal;
};