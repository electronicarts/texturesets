// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "TextureSetAssetParams.h"
#include "TextureSetInfo.h"

#include "TextureSetModule.generated.h"

class FTextureSetProcessingGraph;
class UMaterialExpressionTextureSetSampleParameter;
class UTextureSetDefinition;
class FTextureSetSampleFunctionBuilder;

// Abstract class for texture set modules. Modules provide a mechanism for extending textures sets with additional functionality.
UCLASS(Abstract, Optional, EditInlineNew, DefaultToInstanced, CollapseCategories)
class TEXTURESETSCOMMON_API UTextureSetModule : public UObject
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	// Overridable duplicate function, allows modules to data patch and update as needed.
	virtual UTextureSetModule* DuplicateModule(UObject* Outer) const { return DuplicateObject<UTextureSetModule>(this, Outer); }

	// Allow children to override name of an instance of this module.
	// Useful for modules which allow multiple instances on the same definition.
	virtual FString GetInstanceName() const { return this->GetClass()->GetName(); }

	// Which classes this module requests be attached to the texture set asset
	virtual void GetAssetParamClasses(TSet<TSubclassOf<UTextureSetAssetParams>>& Classes) const {}

	// Which classes this module requests be attached to the sampler material expression
	virtual void GetSampleParamClasses(TSet<TSubclassOf<UTextureSetSampleParams>>& Classes) const {}

	// Process the source data into the intermediate results
	// Transforms source elements into processed data
	// Sets values of shader constants
	virtual void ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const {}

	// Logic (material graph) for unpacking data
	// Transforms processed data into desired output elements
	virtual void ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
		FTextureSetSampleFunctionBuilder* Builder) const {}

	virtual EDataValidationResult IsDefinitionValid(const UTextureSetDefinition* Definition, FDataValidationContext& Context) const { return EDataValidationResult::NotValidated; }
#endif
};
