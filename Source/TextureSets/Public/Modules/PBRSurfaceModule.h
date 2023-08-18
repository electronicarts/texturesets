// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "PBRSurfaceModule.generated.h"

UENUM()
enum class EPBRParamaterization
{
	Basecolor_Metal,
	Albedo_Spec,
	Dielectric,
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
	// TODO: Support alternative normal sample outputs
	//World,
	//SurfaceGradient,
};

UCLASS()
class UPBRSampleParams : public UTextureSetSampleParams
{
	GENERATED_BODY()
public:
	// TODO: Support conversion on sampling
	//UPROPERTY(EditAnywhere);
	//EPBRParamaterization ParameterizationOutput;
	//
	//UPROPERTY(EditAnywhere);
	//EPBRNormalSpace NormalSpaceOutput;

	UPROPERTY(EditAnywhere);
	EPBRMicrosurface MicrosurfaceOutput;
};

UCLASS()
class UPBRSurfaceModule : public UTextureSetModule
{
	GENERATED_BODY()
public:

	const FName MetalName = "Metal";
	const FName BaseColorName = "BaseColor";
	const FName AlbedoName = "Albedo";
	const FName SpecularName = "Specular";
	const FName RoughnessName = "Roughness";
	const FName SmoothnessName = "Smoothness";
	const FName TangentNormalName = "TangentNormal";
	const FName WorldNormalName = "WorldNormal";
	const FName SurfaceGradientName = "SurfaceGradient";

	virtual bool AllowMultiple() const override { return false; }
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const override { return UPBRSampleParams::StaticClass(); }

	virtual void BuildModuleInfo(FTextureSetDefinitionModuleInfo& Info) const override;

	virtual void BuildSamplingInfo(
		FTextureSetDefinitionSamplingInfo& SamplingInfo,
		const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;

	virtual void Process(FTextureSetProcessingContext& Context) const override;

	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;

#if WITH_EDITOR
	virtual void GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
		FTextureSetMaterialGraphBuilder& Builder) const override;
#endif

private:
	UPROPERTY(EditAnywhere)
	EPBRParamaterization Paramaterization;

	UPROPERTY(EditAnywhere)
	EPBRMicrosurface Microsurface;

	UPROPERTY(EditAnywhere)
	EPBRNormal Normal;

	UPROPERTY(EditAnywhere)
	bool bEnableNDFPrefiltering; // Not implemented
};