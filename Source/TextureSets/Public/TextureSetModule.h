// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetAssetParams.h"
#include "TextureSetInfo.h"
#include "TextureSetMaterialGraphBuilder.h"
#include "TextureSetProcessingGraph.h"

#include "TextureSetModule.Generated.h"

// Abstract class for texture set modules. Modules provide a mechanism for extending textures sets with additional functionality.
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UTextureSetModule : public UObject
{
	GENERATED_BODY()
public:
	// Can there be more than one of these modules on a definition?
	virtual bool AllowMultiple() const { return false; } // TODO: Not validated/enforced

	// Allow children to override name of an instance of this module.
	// Useful for modules which allow multiple instances on the same definition.
	virtual FString GetInstanceName() const { return this->GetClass()->GetName(); }

	// Which class this module uses to attach parameters to a texture set asset instance
	virtual TSubclassOf<UTextureSetAssetParams> GetAssetParamClass() const { return nullptr; }

	// Which class this module uses to attach parameters to the sampler material expression
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const { return nullptr; }

#if WITH_EDITOR
	// Process the source data into the intermediate results
	// Transforms source elements into processed data
	// Sets values of shader constants
	virtual void GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const {}

	// Compute a hash for the sampling graph. If this hash changes, it triggers the sampling graph to be re-generated.
	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const { return 0; }

	// Logic (material graph) for unpacking data
	// Transforms processed data into desired output elements
	virtual void GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
		FTextureSetMaterialGraphBuilder& Builder) const {}
#endif
};
