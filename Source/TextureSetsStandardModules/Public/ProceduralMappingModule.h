// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetModule.h"
#include "ProceduralMappingModule.generated.h"

// Affects the sampler node, adding the option to sample texture sets with tri-planar mapping.
UCLASS()
class UProceduralMappingModule : public UTextureSetModule
{
	GENERATED_BODY()
public:
	virtual void GetSampleParamClasses(TSet<TSubclassOf<UTextureSetSampleParams>>& Classes) const override;

	virtual int32 ComputeSamplingHash(const FTextureSetAssetParamsCollection* SampleParams) const override;
	virtual void ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
		FTextureSetMaterialGraphBuilder* Builder) const override;
};
