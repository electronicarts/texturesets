// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetModule.h"
#include "BakerModule.generated.h"

class UStaticMesh;

USTRUCT()
struct FBakerInstanceParam
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
	UStaticMesh* SourceMesh = nullptr;
	
	UPROPERTY(EditAnywhere)
	uint32 BakedTextureWidth = 512;
	
	UPROPERTY(EditAnywhere)
	uint32 BakedTextureHeight = 512;
};

UCLASS()
class UBakerAssetParams : public UTextureSetAssetParams
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, EditFixedSize, meta=(ReadOnlyKeys))
	TMap<FName, FBakerInstanceParam> BakerParams;

	virtual void FixupData(UObject* Outer) override;
};

// Allows users to define a custom input texture map that can be packed, and unpacked with no extra processing.
UCLASS()
class UBakerModule : public UTextureSetModule
{
	GENERATED_BODY()
public:

	virtual FString GetInstanceName() const override { return ElementName.ToString(); }

	virtual TSubclassOf<UTextureSetAssetParams> GetAssetParamClass() const { return UBakerAssetParams::StaticClass(); }

	virtual void ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const override;

	virtual int32 ComputeSamplingHash(const FTextureSetAssetParamsCollection* SampleParams) const override;

	virtual void ConfigureSamplingGraphBuilder(
		const FTextureSetAssetParamsCollection* SampleParams,
		FTextureSetMaterialGraphBuilder* Builder) const override;

	UPROPERTY(EditAnywhere)
	FName ElementName;
};