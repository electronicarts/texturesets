// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SharedPointer.h"
#include "ImageCore.h"
#include "TextureSetCooker.h"
#include "TextureSetDerivedData.h"

#include "TextureSet.generated.h"

class UTexture;
class UTextureSetDefinition;

class FSharedImage : public FImage, public FRefCountBase
{
public:
	FSharedImage(int32 InSizeX, int32 InSizeY, int32 InNumSlices, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace)
		: FImage(InSizeX, InSizeY, InNumSlices, InFormat, InGammaSpace)
	{
	}
};

UCLASS(BlueprintType, hidecategories = (Object))
class TEXTURESETS_API UTextureSet : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class TextureSetCooker;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTextureSetDefinition> Definition;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, EditFixedSize)
	TMap<FName, TObjectPtr<class UTexture>> SourceTextures;

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

	virtual void PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Compute the hash key for a specific hashed texture.
	FString ComputePackedTextureKey(int PackedTextureDefIndex) const;

	// Compute the hash key for the entire texture set (including all hashed textures)
	FString ComputeTextureSetDataKey() const;

	void UpdateDerivedData();
	UTextureSetDerivedData* GetDerivedData() { return DerivedData.Get(); }
	UTexture* GetDerivedTexture(int Index) { return DerivedTextures[Index].Get(); }

	void FixupData();

	// For debugging, allow the user to manually change a value that doesn't affect the logic,
	// but is hashed. Forces a regeneration of the data when a new unique value is entered.
	UPROPERTY(EditAnywhere, AdvancedDisplay)
	FString UserKey;

private:

	UPROPERTY(AdvancedDisplay, VisibleAnywhere)
	TObjectPtr<UTextureSetDerivedData> DerivedData;

	UPROPERTY(AdvancedDisplay, VisibleAnywhere)
	TArray<TObjectPtr<UTexture>> DerivedTextures;


};
