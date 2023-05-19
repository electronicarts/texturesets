// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetDefinition.h"
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

// Allows users to define a custom set of elements that can be packed, and unpacked with no extra processing.
UCLASS(Abstract, Blueprintable)
class UTextureSetElementCollection : public UTextureSetDefinitionModule
{
	GENERATED_BODY()
public:
	virtual bool AllowMultiple() { return bAllowMultiple; }

	virtual TArray<TextureSetTextureDef> GetSourceTextures() const override;
	virtual void CollectSampleOutputs(TMap<FName, EMaterialValueType>& Results, const UMaterialExpressionTextureSetSampleParameter* SampleParams) const override;

protected:
	UPROPERTY(EditDefaultsOnly)
	bool bAllowMultiple;

	UPROPERTY(EditDefaultsOnly)
	TArray<FElementDefinition> Elements;
};