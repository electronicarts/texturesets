// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TextureSetPackedTextureDef.h"
#include "TextureSetDefinition.generated.h"

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

struct TextureSetParameterDef
{
public:
	FName Name;
	ESourceParameterType ParameterType;
	FVector4 DefaultValue;
};

class OutputElementDef
{
public:
	// Name on the sampler output pin
	FName Name;

	// Material pin type
	EMaterialValueType Type;
};

// Class which is instanced on each texture set sampler, and then provided to the module
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, BlueprintType, Blueprintable, CollapseCategories)
class UTextureSetSampleParams : public UObject
{
	GENERATED_BODY()
};

UCLASS(Abstract, EditInlineNew, DefaultToInstanced, BlueprintType, Blueprintable, CollapseCategories)
class UTextureSetDefinitionModule : public UObject
{
	GENERATED_BODY()
public:
	// Can there be more than one of these modules on a definition?
	virtual bool AllowMultiple() { return false; }

	// Allow children to override name of an instance of this module.
	// Useful for modules which allow multiple instances on the same definition.
	virtual FString GetInstanceName() const { return this->GetClass()->GetName(); }

	// Parameters stored on the texture set instance, which feed into the cooking step
	virtual TArray<TextureSetParameterDef> GetSourceParameters() const { return TArray<TextureSetParameterDef> {}; }
	// Input texture maps which are to be processed
	virtual TArray<TextureSetTextureDef> GetSourceTextures() const { return TArray<TextureSetTextureDef> {}; }
	// Processed texture maps which are to be packed
	virtual TArray<TextureSetTextureDef> GetProcessedTextures() const { return GetSourceTextures(); }

	// Shader constants required for sampling
	virtual TMap<FString, EMaterialValueType> GetShaderConstants() const { return TMap<FString, EMaterialValueType> {}; }
	
	// Which class to attach to the sampler node to store our sampling parameters
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const { return nullptr; }
	
	// Output pins on the sampler node
	virtual TArray<OutputElementDef> GetOutputElements(const UTextureSetSampleParams* SampleParams) const { return TArray<OutputElementDef> {}; }

	// Process the source data into the intermediate results
	// Transforms source elements into processed data
	// TODO: Define arguments and return values
	virtual void Process() {}

	// Logic (material graph) for unpacking data
	// Transforms processed data into desired output elements
	// TODO: Define arguments and return values
	virtual void Sample() {}
};

// TODO: Remove
USTRUCT()
struct FTextureDefinitionInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FString TextureTypes;

	UPROPERTY(EditAnywhere, Category=Components)
	bool R = true;
	UPROPERTY(EditAnywhere, Category=Components)
	bool G = false;
	UPROPERTY(EditAnywhere, Category=Components)
	bool B = false;
	UPROPERTY(EditAnywhere, Category=Components)
	bool A = false;
};

UCLASS()
class TEXTURESETS_API UTextureSetDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// TODO: Remove
	UPROPERTY(EditAnywhere)
	TArray<FTextureDefinitionInfo> Items;
	
	UPROPERTY(EditAnywhere)
	TArray<UTextureSetDefinitionModule*> Modules;

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

	TArray<TextureSetParameterDef> GetSourceParameters() const;
	TArray<TextureSetTextureDef> GetSourceTextures() const;
	TArray<TextureSetTextureDef> GetProcesedTextures() const;
	TMap<FString, EMaterialValueType> GetShaderConstants() const;
	TArray<OutputElementDef> GetOutputElements(TArray<UTextureSetSampleParams*> SampleParamsArray) const;
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
