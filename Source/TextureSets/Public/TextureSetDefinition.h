// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetPackedTextureDef.h"
#include "TextureSetMaterialGraphBuilder.h"
#include "TextureSetDefinition.generated.h"

class UMaterialExpressionTextureSetSampleParameter;

// A texture map input
struct TextureSetTextureDef
{
public:
	FName Name;
	bool SRGB; // Used for correct packing and sampling
	uint8 ChannelCount; // between 1 and 4
	FVector4 DefaultValue; // Used as a fallback if this map is not provided
};

// Data class which is instanced on each texture set asset. Module can use this to allow users to configure asset settings.
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UTextureSetAssetParams : public UObject
{
	GENERATED_BODY()
};

// Data class which is instanced on each texture set sampler material expression. Module can use this to allow users to configure sample settings.
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UTextureSetSampleParams : public UObject
{
	GENERATED_BODY()
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
	FVector4 GetDefaultColor(int index) const;
	const TArray<FTextureSetPackedTextureDef> GetPackedTextures() const { return PackedTextureDefs; }
	const int NumPackedTextures() const { return PackedTextureDefs.Num(); }

private:
	struct TextureSetPackedTextureInfo
	{
	public:
		FName ProcessedTextures[4];
		int ProessedTextureChannels[4];
		bool SRGB[4];
		FVector4 DefaultColor;
	};

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

class ITextureSetTexture
{
public:
	virtual ~ITextureSetTexture() {}

	virtual int GetWidth() const = 0;
	virtual int GetHeight() const = 0;
	virtual int GetChannels() const = 0;
	virtual void Prepare() {}; // Optional for any sub-classes that want to pre-process data before sampling
	virtual float GetPixel(int X, int Y, int Channel) const = 0;
};

class FImageWrapper : public ITextureSetTexture
{
public:
	FImageWrapper(TSharedRef<FImage> I) : Image(I)
	{}

	TSharedRef<FImage> Image;

	virtual int GetWidth() const override { return Image->GetWidth(); }
	virtual int GetHeight() const override { return Image->GetHeight(); }
	virtual int GetChannels() const override{ return 3; } // TODO
	
	virtual float GetPixel(int X, int Y, int Channel) const override
	{
		// TODO: Support more than just RGBA32F textures
		return Image->AsRGBA32F()[X * GetWidth() + Y].Component(Channel);
	}
};

class IDefaultValue : public ITextureSetTexture
{
public:
	IDefaultValue(FLinearColor Col, int Chan)
		: Color(Col)
		, Channels(Chan)
	{}

	virtual int GetWidth() const override { return 4; }
	virtual int GetHeight() const override { return 4; }
	virtual int GetChannels() const override{ return 3; } // TODO

	virtual float GetPixel(int X, int Y, int Channel) const override
	{
		check(Channel < Channels);
		return Color.Component(Channel);
	}

private:
	FLinearColor Color;
	int Channels;
};

class FImageOperator : public ITextureSetTexture
{
public:
	FImageOperator(const TSharedRef<ITextureSetTexture> I)
		: SourceImage(I)
	{}

	virtual int GetWidth() const override { return SourceImage->GetWidth(); }
	virtual int GetHeight() const override { return SourceImage->GetHeight(); }
	virtual int GetChannels() const override { return SourceImage->GetChannels(); }

	const TSharedRef<ITextureSetTexture> SourceImage;
};

class FImageOperatorInvert : public FImageOperator
{
public:
	FImageOperatorInvert(TSharedRef<ITextureSetTexture> I) : FImageOperator(I)
	{}

	virtual float GetPixel(int X, int Y, int Channel) const override
	{
		return 1.0f - SourceImage->GetPixel(X, Y, Channel);
	}
};

class FTextureSetProcessingContext
{
	friend class UTextureSetDefinition;

public:
	void AddProcessedTexture(FName Name, TSharedRef<ITextureSetTexture> Texture) { ProcessedTextures.Add(Name, Texture); }
	bool HasSourceTexure(FName Name) { return SourceTextures.Contains(Name); }
	const TSharedRef<ITextureSetTexture> GetSourceTexture(FName Name) const { return SourceTextures.FindChecked(Name); }

private:
	TMap<FName, TSharedRef<ITextureSetTexture>> SourceTextures;
	TMap<FName, TSharedRef<ITextureSetTexture>> ProcessedTextures;
	TMap<FName, FVector4> MaterialParams;
};

// Abstract class for texture set modules. Modules provide a mechanism for extending textures sets with additional functionality.
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UTextureSetDefinitionModule : public UObject
{
	GENERATED_BODY()
public:
	// Can there be more than one of these modules on a definition?
	virtual bool AllowMultiple() { return false; } // TODO: Not validated/enforced

	// Allow children to override name of an instance of this module.
	// Useful for modules which allow multiple instances on the same definition.
	virtual FString GetInstanceName() const { return this->GetClass()->GetName(); }

	// Which class this module uses to attach parameters to a texture set asset instance
	virtual TSubclassOf<UTextureSetAssetParams> GetAssetParamClass() const { return nullptr; }

	// Which class this module uses to attach parameters to the sampler material expression
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const { return nullptr; }

	// Use in subclasses to add to the shared info
	virtual void BuildSharedInfo(TextureSetDefinitionSharedInfo& Info) {};

	// Use in subclasses to add to the sampling info
	virtual void BuildSamplingInfo(TextureSetDefinitionSamplingInfo& SamplingInfo, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) {};

	// Compute a hash for the module processing. If this hash changes, it triggers a texture-sets to be re-processed.
	virtual int32 ComputeProcessingHash() { return 0; }

#if WITH_EDITOR
	// Process the source data into the intermediate results
	// Transforms source elements into processed data
	// Sets values of shader constants
	virtual void Process(FTextureSetProcessingContext& Context) {}
#endif

	// Compute a hash for the sampling graph. If this hash changes, it triggers the sampling graph to be re-generated.
	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) { return 0; }

#if WITH_EDITOR
	// Logic (material graph) for unpacking data
	// Transforms processed data into desired output elements
	virtual void GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const {}
#endif
};

// The texture set definition. Definitions consist primarily of a list of modules, and a packing definition.
// The definition is configurable, and drives most other aspects of the texture-set from import to packing to sampling.
UCLASS()
class TEXTURESETS_API UTextureSetDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// Suffixes used when addressing specific channels of a texture by name
	// Defined as {".r", ".g", ".b", ".a"}
	static const FString ChannelSuffixes[4];

	// UObject Overrides
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;


	UFUNCTION(CallInEditor)
	TArray<FName> GetUnpackedChannelNames() const;

	const TextureSetDefinitionSharedInfo GetSharedInfo() const;
	const TextureSetDefinitionSamplingInfo GetSamplingInfo(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const;
	const TextureSetPackingInfo GetPackingInfo() const;

	TArray<TSubclassOf<UTextureSetAssetParams>> GetRequiredAssetParamClasses() const;
	TArray<TSubclassOf<UTextureSetSampleParams>> GetRequiredSampleParamClasses() const;

	// Get the default packed texture for a specific packed texture index
	UTexture* GetDefaultPackedTexture(int index) const;

	int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression);
#if WITH_EDITOR
	void GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression, FTextureSetMaterialGraphBuilder& Builder) const;
#endif
private:

	UPROPERTY(EditAnywhere)
	TArray<UTextureSetDefinitionModule*> Modules;

	UPROPERTY(EditAnywhere, meta=(TitleProperty="CompressionSettings"))
	TArray<FTextureSetPackedTextureDef> PackedTextures;

	UPROPERTY(VisibleAnywhere, DuplicateTransient)
	TArray<UTexture*> DefaultTextures;

#if WITH_EDITOR
	void UpdateDefaultTextures();
#endif
};