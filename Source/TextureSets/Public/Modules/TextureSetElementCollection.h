// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "TextureSetElementCollection.generated.h"

USTRUCT(BlueprintType)
struct FElementDefinition
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere)
	FName ElementName;

	UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties))
	FTextureSetSourceTextureDef ElementDef;
};

FORCEINLINE uint32 GetTypeHash(const FElementDefinition& Def)
{
	uint32 Hash = 0;
	Hash = HashCombine(Hash, GetTypeHash(Def.ElementName.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(Def.ElementDef));
	return int32();
}

// Allows users to define a custom set of elements that can be packed, and unpacked with no extra processing.
UCLASS(Abstract, Blueprintable)
class UTextureSetElementCollection : public UTextureSetModule
{
	GENERATED_BODY()
public:
	virtual bool AllowMultiple() const override { return bAllowMultiple; }
	
#if WITH_EDITOR
	virtual void ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;

	virtual void ConfigureSamplingGraphBuilder(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
		FTextureSetMaterialGraphBuilder* Builder) const override;
#endif

protected:
	UPROPERTY(EditDefaultsOnly)
	bool bAllowMultiple = false;

	UPROPERTY(EditDefaultsOnly)
	TArray<FElementDefinition> Elements;
};