// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetPackedTextureDef.h"

#include "TextureSetInfo.generated.h"

// Processed texture map ready for packing
USTRUCT()
struct FTextureSetProcessedTextureDef
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere)
	bool SRGB; // Used for correct packing and sampling

	UPROPERTY(VisibleAnywhere)
	uint8 ChannelCount; // between 1 and 4
};

inline uint32 GetTypeHash(const FTextureSetProcessedTextureDef& Def)
{
	return HashCombine(GetTypeHash(Def.SRGB), GetTypeHash(Def.ChannelCount));
}

// A texture map input
USTRUCT()
struct FTextureSetSourceTextureDef : public FTextureSetProcessedTextureDef
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	FVector4 DefaultValue; // Used as a fallback if this map is not provided
};

inline uint32 GetTypeHash(const FTextureSetSourceTextureDef& Def)
{
	return HashCombine(GetTypeHash((FTextureSetProcessedTextureDef)Def), GetTypeHash(Def.DefaultValue));
}

// Info which is provided by the definition modules, and is needed both for cooking and sampling from a texture set
USTRUCT()
struct FTextureSetDefinitionModuleInfo
{
	friend class UTextureSetDefinition;

	GENERATED_BODY()
public:
	virtual ~FTextureSetDefinitionModuleInfo() {}

	const TMap<FName, FTextureSetProcessedTextureDef>& GetSourceTextures() const { return SourceTextures; }
	const TMap<FName, FTextureSetProcessedTextureDef>& GetProcessedTextures() const { return ProcessedTextures; }

private:
	// Input texture maps which are to be processed
	UPROPERTY(VisibleAnywhere)
	TMap<FName, FTextureSetProcessedTextureDef> SourceTextures;

	// Processed texture maps which are to be packed
	UPROPERTY(VisibleAnywhere)
	TMap<FName, FTextureSetProcessedTextureDef> ProcessedTextures;
};

// Info used for packing, not exposed to the modules
UENUM()
enum class ETextureSetTextureChannelEncoding  : uint8
{
	Linear_Raw,
	Linear_RangeCompressed,
	SRGB
};

USTRUCT()
struct FTextureSetPackedChannelInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	FName ProcessedTexture;

	UPROPERTY(VisibleAnywhere)
	int ProessedTextureChannel;

	UPROPERTY(VisibleAnywhere)
	ETextureSetTextureChannelEncoding ChannelEncoding;
};

USTRUCT()
struct FTextureSetPackedTextureInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	FTextureSetPackedChannelInfo ChannelInfo[4];

	UPROPERTY(VisibleAnywhere)
	int ChannelCount;

	UPROPERTY(VisibleAnywhere)
	bool HardwareSRGB;

	UPROPERTY(VisibleAnywhere)
	FName RangeCompressMulName;

	UPROPERTY(VisibleAnywhere)
	FName RangeCompressAddName;
};

USTRUCT()
struct FTextureSetPackingInfo
{
	friend class UTextureSetDefinition;

	GENERATED_BODY()
public:

	const int NumPackedTextures() const { return PackedTextureDefs.Num(); }

	const FTextureSetPackedTextureDef& GetPackedTextureDef(int Index) const { return PackedTextureDefs[Index]; }
	const FTextureSetPackedTextureInfo& GetPackedTextureInfo(int Index) const { return PackedTextureInfos[Index]; }

private:
	UPROPERTY(VisibleAnywhere)
	TArray<FTextureSetPackedTextureDef> PackedTextureDefs;

	UPROPERTY(VisibleAnywhere)
	TArray<FTextureSetPackedTextureInfo> PackedTextureInfos;
};

// Info which is needed to generate the sampling graph for a texture set
USTRUCT()
struct FTextureSetDefinitionSamplingInfo
{
	friend class UTextureSetDefinition;

	GENERATED_BODY()
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