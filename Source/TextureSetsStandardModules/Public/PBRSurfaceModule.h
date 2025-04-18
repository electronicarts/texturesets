// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "PBRSurfaceModule.generated.h"

UENUM()
enum class EPBRParamaterization
{
	None,
	Basecolor_Metal,
	Albedo_Spec,
	Dielectric,
};

UENUM()
enum class EPBRMicrosurface
{
	None,
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

UCLASS()
class UPBRSampleParams : public UTextureSetSampleParams
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="PBRParams");
	EPBRMicrosurface MicrosurfaceOutput = EPBRMicrosurface::Roughness;

	UPROPERTY(EditAnywhere, Category="PBRParams");
	EPBRNormalSpace NormalOutput = EPBRNormalSpace::Tangent;
};

UCLASS()
class UPBRAssetParams : public UTextureSetAssetParams
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="PBRParams");
	bool bFlipNormalGreen;
};

// Defines common, standardized PBR shading elements - albedo, roughness, metallic, and normals.
UCLASS()
class UPBRSurfaceModule : public UTextureSetModule
{
	GENERATED_BODY()
public:

	const FName MetalName = "Metallic";
	const FName BaseColorName = "BaseColor";
	const FName AlbedoName = "Albedo";
	const FName SpecularName = "Specular";
	const FName RoughnessName = "Roughness";
	const FName SmoothnessName = "Smoothness";
	const FName TangentNormalName = "TangentNormal";
	const FName WorldNormalName = "WorldNormal";
	const FName SurfaceGradientName = "SurfaceGradient";

	virtual void GetAssetParamClasses(TSet<TSubclassOf<UTextureSetAssetParams>>& Classes) const override;
	virtual void GetSampleParamClasses(TSet<TSubclassOf<UTextureSetSampleParams>>& Classes) const override;

	virtual void ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual void ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
		FTextureSetSampleFunctionBuilder* Builder) const override;

private:
	UPROPERTY(EditAnywhere, Category="PBRSurface")
	EPBRParamaterization Paramaterization = EPBRParamaterization::Basecolor_Metal;

	UPROPERTY(EditAnywhere, Category="PBRSurface")
	EPBRMicrosurface Microsurface = EPBRMicrosurface::Roughness;

	UPROPERTY(EditAnywhere, Category="PBRSurface")
	EPBRNormal Normal = EPBRNormal::Tangent;
};