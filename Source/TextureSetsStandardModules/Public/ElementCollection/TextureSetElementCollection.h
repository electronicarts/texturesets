// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "TextureSetElementCollection.generated.h"

UCLASS()
class UTextureSetElementCollectionAsset : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="ElementCollection")
	TMap<FName, FTextureSetSourceTextureDef> Elements;
};

// Contains one or more custom elements in their own data asset, for reusing sets of custom elements between definitions across the project.
UCLASS()
class UTextureSetElementCollectionModule : public UTextureSetModule
{
	GENERATED_BODY()
public:
	virtual FString GetInstanceName() const;

	virtual void ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual int32 ComputeSamplingHash(const FTextureSetAssetParamsCollection* SampleParams) const override;

	virtual void ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
		FTextureSetMaterialGraphBuilder* Builder) const override;

	UPROPERTY(EditAnywhere, Category="ElementCollection")
	UTextureSetElementCollectionAsset* Collection;
};
