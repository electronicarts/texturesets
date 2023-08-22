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

FORCEINLINE uint32 GetTypeHash(const FElementDefinition& Def)
{
	uint32 Hash = 0;
	Hash = HashCombine(Hash, GetTypeHash(Def.ElementName));
	Hash = HashCombine(Hash, GetTypeHash(Def.SRGB));
	Hash = HashCombine(Hash, GetTypeHash(Def.ChannelCount));
	Hash = HashCombine(Hash, GetTypeHash(Def.DefaultValue));
	return int32();
}

// Allows users to define a custom set of elements that can be packed, and unpacked with no extra processing.
UCLASS(Abstract, Blueprintable)
class UTextureSetElementCollection : public UTextureSetModule
{
	GENERATED_BODY()
public:
	virtual bool AllowMultiple() const override { return bAllowMultiple; }
	
	virtual void GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;
#if WITH_EDITOR
	virtual void GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
		FTextureSetMaterialGraphBuilder& Builder) const override;
#endif

protected:
	UPROPERTY(EditDefaultsOnly)
	bool bAllowMultiple;

	UPROPERTY(EditDefaultsOnly)
	TArray<FElementDefinition> Elements;
};