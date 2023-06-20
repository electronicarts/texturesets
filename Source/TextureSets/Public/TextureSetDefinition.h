// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetPackedTextureDef.h"
#include "TextureSetInfo.h"
#include "TextureSetDefinition.generated.h"

class UMaterialExpressionTextureSetSampleParameter;
class UTextureSet;
class UTextureSetModule;
class UTextureSetAssetParams;
class UTextureSetSampleParams;
class FTextureSetMaterialGraphBuilder;

// The texture set definition. Definitions consist primarily of a list of modules, and a packing definition.
// The definition is configurable, and drives most other aspects of the texture-set from import to packing to sampling.
UCLASS()
class TEXTURESETS_API UTextureSetDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// Suffixes used when addressing specific channels of a texture by name
	// Defined as {".r", ".g", ".b", ".a"}
	static const FString ChannelSuffixes[4];

	// UObject Overrides
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;

	// Gets the list of channel names which have not yet been allocated a spot in the packing definitions.
	UFUNCTION(CallInEditor)
	TArray<FName> GetUnpackedChannelNames() const;

	const TextureSetDefinitionSharedInfo GetSharedInfo() const;
	const TextureSetDefinitionSamplingInfo GetSamplingInfo(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const;
	const TextureSetPackingInfo GetPackingInfo() const;
	const TArray<const UTextureSetModule*> GetModules() const;

	TArray<TSubclassOf<UTextureSetAssetParams>> GetRequiredAssetParamClasses() const;
	TArray<TSubclassOf<UTextureSetSampleParams>> GetRequiredSampleParamClasses() const;

	// Get the default packed texture for a specific packed texture index
	UTexture* GetDefaultPackedTexture(int Index);

	int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression);
#if WITH_EDITOR
	void GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression, FTextureSetMaterialGraphBuilder& Builder) const;
#endif

	virtual void PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext) override;

private:

	UPROPERTY(EditAnywhere)
	TArray<UTextureSetModule*> Modules;

	UPROPERTY(EditAnywhere, meta=(TitleProperty="CompressionSettings"))
	TArray<FTextureSetPackedTextureDef> PackedTextures;

	UPROPERTY(VisibleAnywhere)
	UTextureSet* DefaultTextureSet;

#if WITH_EDITOR
	void UpdateDefaultTextures();
#endif

	void UpdateDependentAssets(bool AutoLoad = false);
};