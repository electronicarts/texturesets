// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetPackedTextureDef.h"

#include "TextureSetInfo.generated.h"

struct FTextureSetPackedTextureDef;
class FTextureSetProcessingGraph;
class UTextureSetModule;

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class ETextureSetTextureFlags : uint8
{
	None = 0 UMETA(Hidden),
	Array = 1 << 0,
	// Additional texture flags such as 1D, 3D, cubemaps, etc. could be supported
};
ENUM_CLASS_FLAGS(ETextureSetTextureFlags);

// Info used for packing, not exposed to the modules
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class ETextureSetChannelEncoding  : uint8
{
	None = 0 UMETA(Hidden),
	RangeCompression = 1 << 0,
	SRGB = 1 << 1,
};
ENUM_CLASS_FLAGS(ETextureSetChannelEncoding);

// Processed texture map ready for packing
USTRUCT()
struct FTextureSetProcessedTextureDef
{
	GENERATED_BODY()

public:
	FTextureSetProcessedTextureDef() : FTextureSetProcessedTextureDef(1, ETextureSetChannelEncoding::None) {}

	FTextureSetProcessedTextureDef(int ChannelCount, ETextureSetChannelEncoding Encoding, ETextureSetTextureFlags Flags = ETextureSetTextureFlags::None)
		: ChannelCount(ChannelCount)
		, Encoding((uint8)Encoding)
		, Flags((uint8)Flags)
	{}

	UPROPERTY(EditAnywhere, meta=(ClampMin = 1, ClampMax = 4))
	uint8 ChannelCount = 1;

	UPROPERTY(EditAnywhere, meta = (Bitmask, BitmaskEnum = "/Script/TextureSets.ETextureSetChannelEncoding"))
	uint8 Encoding = 0;

	UPROPERTY(EditAnywhere, meta = (Bitmask, BitmaskEnum = "/Script/TextureSets.ETextureSetTextureFlags"))
	uint8 Flags = 0;
};

inline uint32 GetTypeHash(const FTextureSetProcessedTextureDef& Def)
{
	uint32 Hash = 0;

	Hash = HashCombine(Hash, GetTypeHash(Def.ChannelCount));
	Hash = HashCombine(Hash, GetTypeHash(Def.Encoding));
	Hash = HashCombine(Hash, GetTypeHash(Def.Flags));

	return Hash;
}

// A texture map input
USTRUCT()
struct FTextureSetSourceTextureDef : public FTextureSetProcessedTextureDef
{
	GENERATED_BODY()

public:
	FTextureSetSourceTextureDef() : FTextureSetSourceTextureDef(1, ETextureSetChannelEncoding::None, FVector4::Zero()) {}

	FTextureSetSourceTextureDef(int ChannelCount, ETextureSetChannelEncoding Encoding, FVector4 DefaultValue)
		: Super(ChannelCount, Encoding)
		, DefaultValue(DefaultValue)
	{}

	UPROPERTY(EditAnywhere)
	FVector4 DefaultValue; // Used as a fallback if this map is not provided
};

inline uint32 GetTypeHash(const FTextureSetSourceTextureDef& Def)
{
	return HashCombine(GetTypeHash((FTextureSetProcessedTextureDef)Def), GetTypeHash(Def.DefaultValue));
}

// Info about and derived from the definition modules. It's needed both for cooking and sampling from a texture set
USTRUCT()
struct FTextureSetDefinitionModuleInfo
{
	friend class UTextureSetDefinition;

	GENERATED_BODY()
public:
#if WITH_EDITOR
	FTextureSetDefinitionModuleInfo(const TArray<const UTextureSetModule*>& Modules);
#endif
	FTextureSetDefinitionModuleInfo();
	virtual ~FTextureSetDefinitionModuleInfo() {}

	const TMap<FName, FTextureSetProcessedTextureDef>& GetSourceTextures() const { return SourceTextures; }
	const TMap<FName, FTextureSetProcessedTextureDef>& GetProcessedTextures() const { return ProcessedTextures; }

	const TArray<const UTextureSetModule*>& GetModules() const { return Modules; }

#if WITH_EDITOR
	const FTextureSetProcessingGraph* GetProcessingGraph() const;
#endif

private:
	UPROPERTY(VisibleAnywhere)
	TArray<const UTextureSetModule*> Modules;

	// Input texture maps which are to be processed
	UPROPERTY(VisibleAnywhere)
	TMap<FName, FTextureSetProcessedTextureDef> SourceTextures;

	// Processed texture maps which are to be packed
	UPROPERTY(VisibleAnywhere)
	TMap<FName, FTextureSetProcessedTextureDef> ProcessedTextures;

#if WITH_EDITOR
	TSharedPtr<FTextureSetProcessingGraph> ProcessingGraph;
#endif
};

USTRUCT()
struct FTextureSetPackedChannelInfo
{
	GENERATED_BODY()

public:
	FTextureSetPackedChannelInfo()
		: ProessedTextureChannel(0)
		, ChannelEncoding((uint8)ETextureSetChannelEncoding::None)
	{}

	UPROPERTY(VisibleAnywhere)
	FName ProcessedTexture;

	UPROPERTY(VisibleAnywhere)
	int ProessedTextureChannel = 0;

	UPROPERTY(VisibleAnywhere)
	uint8 ChannelEncoding; // ETextureSetChannelEncoding
};

USTRUCT()
struct FTextureSetPackedTextureInfo
{
	GENERATED_BODY()

public:
	FTextureSetPackedTextureInfo()
		: ChannelCount(0)
		, HardwareSRGB(false)
		, Flags((uint8)ETextureSetTextureFlags::None)
	{}

	UPROPERTY(VisibleAnywhere)
	FTextureSetPackedChannelInfo ChannelInfo[4];

	UPROPERTY(VisibleAnywhere)
	int ChannelCount;

	UPROPERTY(VisibleAnywhere)
	bool HardwareSRGB;

	UPROPERTY(VisibleAnywhere, meta = (Bitmask, BitmaskEnum = ETextureSetTextureFlags))
	uint8 Flags;

	// Union of the channel encodings for all the packed channels
	UPROPERTY(VisibleAnywhere, meta = (Bitmask, BitmaskEnum = ETextureSetSourceTextureChannelMask))
	uint8 ChannelEncodings;

	UPROPERTY(VisibleAnywhere)
	FName RangeCompressMulName;

	UPROPERTY(VisibleAnywhere)
	FName RangeCompressAddName;
};

// Info about and derived from the packing definition
USTRUCT()
struct FTextureSetPackingInfo
{
	friend class UTextureSetDefinition;

	GENERATED_BODY()
public:
#if WITH_EDITOR
	FTextureSetPackingInfo(const TArray<FTextureSetPackedTextureDef>& PackedTextures, const FTextureSetDefinitionModuleInfo& ModuleInfo);
#endif
	FTextureSetPackingInfo() {}
	const int NumPackedTextures() const { return PackedTextureDefs.Num(); }

	const FTextureSetPackedTextureDef& GetPackedTextureDef(int Index) const { return PackedTextureDefs[Index]; }
	const FTextureSetPackedTextureInfo& GetPackedTextureInfo(int Index) const { return PackedTextureInfos[Index]; }

	// Argument is the channel of the processed texture you want to find (e.g. "Roughness.r" or "Normal.g")
	// Return value is a tuple of the packed texture index and channel where you'll find it
	const TTuple<int, int> GetPackingSource(FName PackedChannel) const { return PackingSource.FindChecked(PackedChannel); }
	bool IsPacked(FName PackedChannel) const { return PackingSource.Contains(PackedChannel); }


private:
	UPROPERTY(VisibleAnywhere)
	TArray<FTextureSetPackedTextureDef> PackedTextureDefs;

	UPROPERTY(VisibleAnywhere)
	TArray<FTextureSetPackedTextureInfo> PackedTextureInfos;

	TMap<FName, TTuple<int, int>> PackingSource;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere)
	TArray<FText> Errors;

	UPROPERTY(VisibleAnywhere)
	TArray<FText> Warnings;
#endif
};