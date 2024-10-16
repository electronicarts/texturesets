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
	friend class UTextureSetTextureSourceProvider;
public:
	UPROPERTY(EditAnywhere, Category="TextureSet", BlueprintReadWrite)
	TObjectPtr<UTextureSetDefinition> Definition;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category="TextureSet", EditFixedSize, meta=(ReadOnlyKeys))
	TMap<FName, FTextureSetSourceTextureReference> SourceTextures;

	UPROPERTY(EditAnywhere, Category="TextureSet")
	FTextureSetAssetParamsCollection AssetParams;

	UPROPERTY(EditAnywhere, Category="TextureSet")
	int32 LODBiasOffset = 0;
#endif

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category="TextureSet")
	void AddSource(FName SourceName, UTexture* Texture);

	UFUNCTION(BlueprintCallable, Category="TextureSet")
	void RemoveSource(FName SourceName);

	UFUNCTION(BlueprintCallable, Category="TextureSet")
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
	virtual void Serialize(FArchive& Ar) override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	virtual bool IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform) override;
	virtual void ClearCachedCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	virtual void ClearAllCachedCookedPlatformData() override;
#endif

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category="TextureSet")
	void FixupData();
	// Fetch from cache, or re-compute the derived data
	void UpdateDerivedData(bool bAllowAsync, bool bStartImmediately = false);
#endif
	const UTextureSetDerivedData* GetDerivedData() const;
	const FString& GetUserKey() const { return UserKey; }

	bool IsDefaultTextureSet() const;

private:

	// For debugging, allow the user to manually change a value that doesn't affect the logic,
	// but is hashed. Forces a regeneration of the data when a new unique value is entered.
	UPROPERTY(EditAnywhere, Category="Debug", AdvancedDisplay)
	FString UserKey;

	// Keep references to source textures that became unused, so if we switch to a definition that can make use of them again, they can be rehooked automatically.
	UPROPERTY(EditAnywhere, Category="Debug", AdvancedDisplay)
	TMap<FName, FTextureSetSourceTextureReference> UnusedSourceTextures;

	UPROPERTY()
	bool bSerializeDerivedData = false;

	// Derived data for the current platform
	UPROPERTY(Transient, DuplicateTransient, VisibleAnywhere, Category="Debug", AdvancedDisplay)
	TObjectPtr<UTextureSetDerivedData> DerivedData;

	FDelegateHandle OnTextureSetDefinitionChangedHandle;

#if WITH_EDITOR
	void OnDefinitionChanged(UTextureSetDefinition* ChangedDefinition);
#endif
};
