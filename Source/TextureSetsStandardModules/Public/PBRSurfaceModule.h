// (c) Electronic Arts. All Rights Reserved.

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
	UPROPERTY(EditAnywhere);
	EPBRMicrosurface MicrosurfaceOutput = EPBRMicrosurface::Roughness;

	UPROPERTY(EditAnywhere);
	EPBRNormalSpace NormalOutput = EPBRNormalSpace::Tangent;
};

UCLASS()
class UPBRAssetParams : public UTextureSetAssetParams
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere);
	bool bFlipNormalGreen;
};

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

	virtual TSubclassOf<UTextureSetAssetParams> GetAssetParamClass() const override { return UPBRAssetParams::StaticClass(); }
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const override { return UPBRSampleParams::StaticClass(); }

	virtual void ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual void ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
		FTextureSetMaterialGraphBuilder* Builder) const override;

private:
	UPROPERTY(EditAnywhere)
	EPBRParamaterization Paramaterization = EPBRParamaterization::Basecolor_Metal;

	UPROPERTY(EditAnywhere)
	EPBRMicrosurface Microsurface = EPBRMicrosurface::Roughness;

	UPROPERTY(EditAnywhere)
	EPBRNormal Normal = EPBRNormal::Tangent;
};