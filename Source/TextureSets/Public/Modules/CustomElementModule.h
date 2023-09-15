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

#if WITH_EDITOR
	virtual void GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;

	virtual void GenerateSamplingGraph(
		const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
		FTextureSetMaterialGraphBuilder& Builder) const override;
#endif

private:
	UPROPERTY(EditAnywhere)
	FName ElementName;

	UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties))
	FTextureSetSourceTextureDef ElementDef;

};