// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetDefinition.h"
#include "CustomElementModule.generated.h"

// Allows users to define a custom input texture map that can be packed, and unpacked with no extra processing.
UCLASS()
class UCustomElementModule : public UTextureSetDefinitionModule
{
	GENERATED_BODY()
public:

	virtual bool AllowMultiple() override { return true; }
	virtual FString GetInstanceName() const  override { return ElementName.ToString(); }

	virtual void BuildSharedInfo(TextureSetDefinitionSharedInfo& Info) override;

	virtual void BuildSamplingInfo(
		TextureSetDefinitionSamplingInfo& SamplingInfo,
		const UMaterialExpressionTextureSetSampleParameter* SampleExpression) override;

	virtual void Process(FTextureSetProcessingContext& Context) override;

	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) override;

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