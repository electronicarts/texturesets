// (c) Electronic Arts. All Rights Reserved.

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

USTRUCT(BlueprintType)
struct FElementDefinition
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category="ElementDefinition")
	FName ElementName;

	UPROPERTY(EditAnywhere, Category="ElementDefinition", meta=(ShowOnlyInnerProperties))
	FTextureSetSourceTextureDef ElementDef;
};

// Old method of doing an element collection, where the user would subclass it with a blueprint.
UCLASS(Deprecated, Abstract, Blueprintable)
class UDEPRECATED_TextureSetElementCollection : public UTextureSetModule
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, Category="ElementCollection")
	TArray<FElementDefinition> Elements;


};