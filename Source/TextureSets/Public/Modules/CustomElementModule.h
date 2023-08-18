// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "CustomElementModule.generated.h"

// Allows users to define a custom input texture map that can be packed, and unpacked with no extra processing.
UCLASS()
class UCustomElementModule : public UTextureSetModule
{
	GENERATED_BODY()
public:

	virtual bool AllowMultiple() const override { return true; }
	virtual FString GetInstanceName() const override { return ElementName.ToString(); }

	virtual void BuildModuleInfo(FTextureSetDefinitionModuleInfo& Info) const override;

	virtual void BuildSamplingInfo(
		FTextureSetDefinitionSamplingInfo& SamplingInfo,
		const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;

	virtual void Process(FTextureSetProcessingContext& Context) const override;

	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;

#if WITH_EDITOR
	virtual void GenerateSamplingGraph(
		const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
		FTextureSetMaterialGraphBuilder& Builder) const override;
#endif

private:
	UPROPERTY(EditAnywhere)
	FName ElementName;

	// Used for correct packing and unpacking
	UPROPERTY(EditAnywhere)
	bool SRGB;

	// between 1 and 4
	UPROPERTY(EditAnywhere)
	uint8 ChannelCount;

	// Used as a fallback if this map is not provided
	UPROPERTY(EditAnywhere)
	FVector4 DefaultValue;

};