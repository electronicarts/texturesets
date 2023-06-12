// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetPackedTextureDef.h"

// A texture map input
struct TextureSetTextureDef
{
public:
	FName Name;
	bool SRGB; // Used for correct packing and sampling
	uint8 ChannelCount; // between 1 and 4
	FVector4 DefaultValue; // Used as a fallback if this map is not provided
};

// Info which is needed both for cooking and sampling from a texture set
struct TextureSetDefinitionSharedInfo
{
	friend class UTextureSetDefinition;
public:
	virtual ~TextureSetDefinitionSharedInfo() {}

	void AddSourceTexture(const TextureSetTextureDef& Texture);
	void AddProcessedTexture(const TextureSetTextureDef& Texture);

	const TArray<TextureSetTextureDef> GetSourceTextures() const;
	const TArray<TextureSetTextureDef> GetProcessedTextures() const;
	const TextureSetTextureDef GetProcessedTextureByName(FName Name) const;

private:
	// Input texture maps which are to be processed
	TArray<TextureSetTextureDef> SourceTextures;
	// Processed texture maps which are to be packed
	TArray<TextureSetTextureDef> ProcessedTextures;
	TMap<FName, int> ProcessedTextureIndicies;
};

// Info used for packing, not exposed to the modules
struct TextureSetPackingInfo
{
	friend class UTextureSetDefinition;
public:

	struct TextureSetPackedChannelInfo
	{
	public:
		FName ProcessedTexture;
		int ProessedTextureChannel;
	};

	struct TextureSetPackedTextureInfo
	{
	public:
		TextureSetPackedChannelInfo ChannelInfo[4];
		int ChannelCount;
		bool AllowHardwareSRGB;
		FVector4 DefaultValue;
	};

	const int NumPackedTextures() const { return PackedTextureDefs.Num(); }

	const FTextureSetPackedTextureDef GetPackedTextureDef(int Index) const { return PackedTextureDefs[Index]; }
	const TextureSetPackedTextureInfo& GetPackedTextureInfo(int Index) const { return PackedTextureInfos[Index]; }

private:
	TArray<FTextureSetPackedTextureDef> PackedTextureDefs;
	TArray<TextureSetPackedTextureInfo> PackedTextureInfos;
};

// Info which is needed to generate the sampling graph for a texture set
struct TextureSetDefinitionSamplingInfo
{
	friend class UTextureSetDefinition;
public:
	void AddMaterialParameter(FName Name, EMaterialValueType Type);
	void AddSampleInput(FName Name, EMaterialValueType Type);
	void AddSampleOutput(FName Name, EMaterialValueType Type);

	const TMap<FName, EMaterialValueType> GetMaterialParameters() const;
	const TMap<FName, EMaterialValueType> GetSampleInputs() const;
	const TMap<FName, EMaterialValueType> GetSampleOutputs() const;

private:
	// Shader constants required for sampling
	TMap<FName, EMaterialValueType> MaterialParameters;
	// Required input pins on the sampler node
	TMap<FName, EMaterialValueType> SampleInputs;
	// Output pins on the sampler node
	TMap<FName, EMaterialValueType> SampleOutputs;
};