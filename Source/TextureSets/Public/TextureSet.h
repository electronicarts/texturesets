// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SharedPointer.h"
#include "ImageCore.h"
#include "TextureSetCooker.h"
#include "TextureSetDerivedData.h"
#include "Materials/MaterialInstance.h"

#include "TextureSet.generated.h"

class UTexture;
class UTextureSetDefinition;

UCLASS(BlueprintType, hidecategories = (Object))
class TEXTURESETS_API UTextureSet : public UObject, public ICustomMaterialParameterInterface
{
	GENERATED_UCLASS_BODY()

	friend class TextureSetCooker;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTextureSetDefinition> Definition;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, EditFixedSize, meta=(ReadOnlyKeys))
	TMap<FName, TObjectPtr<UTexture>> SourceTextures;

	UPROPERTY(EditAnywhere, EditFixedSize)
	TArray<class UTextureSetAssetParams*> AssetParams;

	template <class T>
	const T* GetAssetParams() const
	{
		for (UTextureSetAssetParams* Params : AssetParams)
		{
			const T* P = Cast<T>(Params);

			if (IsValid(P))
			{
				return P;
			}
		}
		return GetDefault<T>(); // Not found, return the default class
	}
#endif

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) override;
#endif

	// ICustomMaterialParameterInterface
	virtual void AugmentMaterialParameters(const FCustomParameterValue& CustomParameter, TArray<FVectorParameterValue>& VectorParameters, TArray<FTextureParameterValue>& TextureParameters) const override;

	// UObject Interface
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

#if WITH_EDITOR
	// Compute the hashed Guid for a specific hashed texture. Used by the DDC to cache the data.
	FGuid ComputePackedTextureDataID(int PackedTextureDefIndex) const;

	// Compute the hashed Guid for the entire texture set (including all hashed textures) Used by the DDC to cache the data.
	FGuid ComputeTextureSetDataId() const;

	void FixupData();
	void UpdateDerivedData();
#endif
	UTextureSetDerivedData* GetDerivedData() { return DerivedData.Get(); }
	UTexture* GetDerivedTexture(int Index) { return DerivedTextures[Index].Get(); }

	// For debugging, allow the user to manually change a value that doesn't affect the logic,
	// but is hashed. Forces a regeneration of the data when a new unique value is entered.
	UPROPERTY(EditAnywhere, Category="Debug")
	FString UserKey;

private:

	UPROPERTY(VisibleAnywhere, Category="Debug")
	TObjectPtr<UTextureSetDerivedData> DerivedData;

	UPROPERTY(VisibleAnywhere, Category="Debug")
	TArray<TObjectPtr<UTexture>> DerivedTextures;

	FDelegateHandle OnTextureSetDefinitionChangedHandle;

	void OnDefinitionChanged(UTextureSetDefinition* ChangedDefinition);
};
