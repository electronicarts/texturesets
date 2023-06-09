// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/ITextureSetTexture.h"
#include "TextureSetInfo.h"
#include "TextureSetMaterialGraphBuilder.h"

#include "TextureSetModule.Generated.h"

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

class FTextureSetProcessingContext
{
	friend class UTextureSetDefinition;

public:
	void AddProcessedTexture(FName Name, TSharedRef<ITextureSetTexture> Texture) { ProcessedTextures.Add(Name, Texture); }
	bool HasSourceTexure(FName Name) { return SourceTextures.Contains(Name); }
	const TSharedRef<ITextureSetTexture> GetSourceTexture(FName Name) const { return SourceTextures.FindChecked(Name); }

	void AddMaterialParam(FName Name, FVector4 Value) { MaterialParams.Add(Name, Value); }
	void AddMaterialParam(FName Name, FVector Value) { MaterialParams.Add(Name, FVector4(Value, 0.0f)); }
	void AddMaterialParam(FName Name, FVector2D Value) { MaterialParams.Add(Name, FVector4(Value, FVector2D::Zero())); }
	void AddMaterialParam(FName Name, float Value) { MaterialParams.Add(Name, FVector4(Value, 0.0f, 0.0f, 0.0f)); }
private:
	TMap<FName, TSharedRef<ITextureSetTexture>> SourceTextures;
	TMap<FName, TSharedRef<ITextureSetTexture>> ProcessedTextures;
	TMap<FName, FVector4> MaterialParams;
};

// Abstract class for texture set modules. Modules provide a mechanism for extending textures sets with additional functionality.
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UTextureSetModule : public UObject
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
