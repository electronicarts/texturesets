// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetDefinition.h"
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
	World,
	SurfaceGradient,
};

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

UCLASS()
class UPBRSurfaceModule : public UTextureSetDefinitionModule
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

	virtual bool AllowMultiple() override { return false; }
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const override { return UPBRSampleParams::StaticClass(); }

	virtual void BuildSharedInfo(TextureSetDefinitionSharedInfo& Info) override;

	virtual void BuildSamplingInfo(
		TextureSetDefinitionSamplingInfo& SamplingInfo,
		const UMaterialExpressionTextureSetSampleParameter* SampleExpression) override;

	virtual void Process(FTextureSetProcessingContext& Context) override;

	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) override;

#if WITH_EDITOR
	virtual void GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
		FTextureSetMaterialGraphBuilder& Builder) const override;
#endif

private:
	UPROPERTY(EditAnywhere)
	EPBRParamaterization Paramaterization; // Partially implemented

	UPROPERTY(EditAnywhere)
	EPBRMicrosurface Microsurface; // Partially implemented

	UPROPERTY(EditAnywhere)
	EPBRNormal Normal; // Partially implemented

	UPROPERTY(EditAnywhere)
	bool bEnableNDFPrefiltering; // Not implemented
};