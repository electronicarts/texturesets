// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Interface_AsyncCompilation.h"
#include "Materials/MaterialInstance.h"
#include "TextureSetAssetParams.h"
#include "TextureSetDerivedData.h"
#include "TextureSetSourceTextureReference.h"

#include "TextureSet.generated.h"

class UTexture;
class UTextureSetDefinition;
class UTextureSetDerivedData;
class FTextureSetProcessingGraph;
struct FTextureSetProcessingContext;

UCLASS(BlueprintType, hidecategories = (Object))
class TEXTURESETS_API UTextureSet : public UObject, public ICustomMaterialParameterInterface, public IInterface_AsyncCompilation
{
	GENERATED_UCLASS_BODY()

	friend class FTextureSetCompilingManager;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTextureSetDefinition> Definition;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, EditFixedSize, meta=(ReadOnlyKeys))
	TMap<FName, FTextureSetSourceTextureReference> SourceTextures;

	UPROPERTY(EditAnywhere)
	FTextureSetAssetParamsCollection AssetParams;
#endif

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable)
	void AddSource(FName SourceName, UTexture* Texture);

	UFUNCTION(BlueprintCallable)
	void RemoveSource(FName SourceName);

	UFUNCTION(BlueprintCallable)
	TArray<FName> GetSourceNames();
#endif

	// IInterface_AsyncCompilation Interface
#if WITH_EDITOR
	virtual bool IsCompiling() const override;
#endif

	// ICustomMaterialParameterInterface
	virtual void AugmentMaterialTextureParameters(const FCustomParameterValue& CustomParameter, TArray<FTextureParameterValue>& TextureParameters) const override;
	virtual void AugmentMaterialVectorParameters(const FCustomParameterValue& CustomParameter, TArray<FVectorParameterValue>& VectorParameters) const override;

	// UObject Interface
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	virtual bool IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform) override;
	virtual void ClearCachedCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	virtual void ClearAllCachedCookedPlatformData() override;
#endif

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable)
	void FixupData();
	// Fetch from cache, or re-compute the derived data
	void UpdateDerivedData(bool bAsync, bool bStartImmediately = false);
#endif
	const FTextureSetDerivedData& GetDerivedData() const { return DerivedData; }
	const FString& GetUserKey() const { return UserKey; }

private:

	// For debugging, allow the user to manually change a value that doesn't affect the logic,
	// but is hashed. Forces a regeneration of the data when a new unique value is entered.
	UPROPERTY(EditAnywhere, AdvancedDisplay)
	FString UserKey;

	UPROPERTY(VisibleAnywhere, AdvancedDisplay)
	FTextureSetDerivedData DerivedData;

	// Keep references to source textures that became unused, so if we switch to a definition that can make use of them again, they can be rehooked automatically.
	UPROPERTY(EditAnywhere, AdvancedDisplay)
	TMap<FName, FTextureSetSourceTextureReference> UnusedSourceTextures;

	FDelegateHandle OnTextureSetDefinitionChangedHandle;

	void NotifyMaterialInstances();

#if WITH_EDITOR
	void OnFinishCook();
	void OnDefinitionChanged(UTextureSetDefinition* ChangedDefinition);
#endif
};
