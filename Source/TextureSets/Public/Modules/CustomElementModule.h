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

	virtual bool AllowMultiple() { return true; }
	virtual FString GetInstanceName() const  override { return ElementName.ToString(); }

	virtual TArray<TextureSetParameterDef> GetSourceParameters() const override
	{
		// No Params
		return TArray<TextureSetParameterDef> {};
	};

	virtual TArray<TextureSetTextureDef> GetSourceTextures() const override
	{
		return TArray<TextureSetTextureDef> { TextureSetTextureDef{ ElementName, SRGB, ChannelCount, DefaultValue } };
	};

	virtual TArray<OutputElementDef> GetOutputElements(const UTextureSetSampleParams* SampleParams) const override
	{
		const EMaterialValueType ValueTypeLookup[4] = { MCT_Float1, MCT_Float2, MCT_Float3, MCT_Float4 };

		return TArray<OutputElementDef> { OutputElementDef{ ElementName, ValueTypeLookup[ChannelCount] } };
	};

private:
	UPROPERTY(EditAnywhere)
	FName ElementName;

	UPROPERTY(EditAnywhere)
	bool SRGB; // Used for correct packing and unpacking

	UPROPERTY(EditAnywhere)
	uint8 ChannelCount; // between 1 and 4

	UPROPERTY(EditAnywhere)
	FVector4 DefaultValue; // Used as a fallback if this map is not provided

};