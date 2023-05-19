// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TextureSetPackedTextureDef.h"
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

	// Input texture maps which are to be processed
	virtual TArray<TextureSetTextureDef> GetSourceTextures() const { return TArray<TextureSetTextureDef> {}; }

	// Processed texture maps which are to be packed
	virtual TArray<TextureSetTextureDef> GetProcessedTextures() const { return GetSourceTextures(); }

	// Shader constants required for sampling
	virtual void CollectShaderConstants(TMap<FName, EMaterialValueType>& Constants, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const {}
	
	// Required input pins on the sampler node
	virtual void CollectSampleInputs(TMap<FName, EMaterialValueType>& Arguments, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const {}

	// Output pins on the sampler node
	virtual void CollectSampleOutputs(TMap<FName, EMaterialValueType>& Results, const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const {}

	// Process the source data into the intermediate results
	// Transforms source elements into processed data
	// Sets values of shader constants
	// TODO: Define arguments and return values
	virtual void Process(const UTextureSetAssetParams* AssetParams) {}

	// Logic (material graph) for unpacking data
	// Transforms processed data into desired output elements
	// TODO: Define arguments and return values
	virtual void Sample(const UTextureSetSampleParams* SampleParams) {}
};

UCLASS()
class TEXTURESETS_API UTextureSetDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<UTextureSetDefinitionModule*> Modules;

	UPROPERTY()
	TObjectPtr<UMaterialFunction> SamplingMaterialFunction;
	UPROPERTY(EditAnywhere, meta=(TitleProperty="CompressionSettings"))
	TArray<FTextureSetPackedTextureDef> PackedTextures;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

	TObjectPtr<UMaterialFunction> GenerateMaterialFunction(class UMaterialExpressionTextureSetSampleParameter* ExpressionInstance);

	UFUNCTION(CallInEditor)
	TArray<FName> GetUnpackedChannelNames() const;

	TArray<TextureSetTextureDef> GetSourceTextures() const;
	TArray<TextureSetTextureDef> GetProcessedTextures() const;
	TMap<FName, EMaterialValueType> GetShaderConstants(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const;
	TMap<FName, EMaterialValueType> GetSampleArguments(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const;
	TMap<FName, EMaterialValueType> GetSampleResults(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const;
	
	TArray<TSubclassOf<UTextureSetAssetParams>> GetRequiredAssetParamClasses() const;
	TArray<TSubclassOf<UTextureSetSampleParams>> GetRequiredSampleParamClasses() const;
};

UCLASS()
class UTextureSetDefinitionFactory : public UFactory
{
	GENERATED_BODY()
	UTextureSetDefinitionFactory(const FObjectInitializer& ObjectInitializer);

public:
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
