// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
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

enum class ESourceParameterType
{
	Scalar,
	Vector2,
	Vector3,
	Vector4,
	Color,
	Boolean,
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

struct TextureSetPackingInfo
{
	friend class UTextureSetDefinition;
public:
	FVector4 GetDefaultColor(int index);

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

UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UTextureSetDefinitionModule : public UObject
{
	GENERATED_BODY()
public:
	// Can there be more than one of these modules on a definition?
	virtual bool AllowMultiple() { return false; }

	// Allow children to override name of an instance of this module.
	// Useful for modules which allow multiple instances on the same definition.
	virtual FString GetInstanceName() const { return this->GetClass()->GetName(); }

	// Which class this module uses to attach parameters to a texture set asset instance
	virtual TSubclassOf<UTextureSetAssetParams> GetAssetParamClass() const { return nullptr; }

	// Which class this module uses to attach parameters to the sampler material expression
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const { return nullptr; }

	virtual void BuildSharedInfo(TextureSetDefinitionSharedInfo& Info) {};

	virtual void BuildSamplingInfo(TextureSetDefinitionSamplingInfo& SamplingInfo, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) {};

	virtual int32 ComputeProcessingHash() { return 0; }

	// Process the source data into the intermediate results
	// Transforms source elements into processed data
	// Sets values of shader constants
	// TODO: Define arguments and return values
	virtual void Process(const UTextureSetAssetParams* AssetParams) {}

	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) { return 0; }

	// Logic (material graph) for unpacking data
	// Transforms processed data into desired output elements
	virtual void GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const {}
};

UCLASS()
class TEXTURESETS_API UTextureSetDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// Suffixes used when addressing specific channels of a texture by name
	// Defined as {".r", ".g", ".b", ".a"}
	static const FString ChannelSuffixes[4];

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	UFUNCTION(CallInEditor)
	TArray<FName> GetUnpackedChannelNames() const;

	const TextureSetDefinitionSharedInfo GetSharedInfo() const;
	const TextureSetDefinitionSamplingInfo GetSamplingInfo(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const;
	const TextureSetPackingInfo GetPackingInfo() const;

	TArray<TSubclassOf<UTextureSetAssetParams>> GetRequiredAssetParamClasses() const;
	TArray<TSubclassOf<UTextureSetSampleParams>> GetRequiredSampleParamClasses() const;

	UTexture* GetDefaultPackedTexture(int index) const;

	int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression);

	void GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression, FTextureSetMaterialGraphBuilder& Builder) const;

public: // TODO: Make Private

	UPROPERTY(EditAnywhere)
	TArray<UTextureSetDefinitionModule*> Modules;

	UPROPERTY(EditAnywhere, meta=(TitleProperty="CompressionSettings"))
	TArray<FTextureSetPackedTextureDef> PackedTextures;


	TArray<UTexture*> DefaultTextures;

	void UpdateDefaultTextures();
};

UCLASS()
class UTextureSetDefinitionFactory : public UFactory
{
	GENERATED_BODY()
	UTextureSetDefinitionFactory(const FObjectInitializer& ObjectInitializer);

public:
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};