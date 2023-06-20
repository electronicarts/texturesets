// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SharedPointer.h"
#include "ImageCore.h"
#include "TextureSetCooker.h"

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

USTRUCT()
struct TEXTURESETS_API FPackedTextureData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTexture> Texture;

	// Material parameters that were generated along with this packed texture
	UPROPERTY(VisibleAnywhere)
	TMap<FName, FVector4> MaterialParameters;

	// Hashed value computed when this texture was built
	UPROPERTY(VisibleAnywhere)
	FString Key;
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

	virtual void PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext) override;
	virtual void PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void FixupData();

	// Compute the hash key for a specific hashed texture.
	FString ComputePackedTextureKey(int PackedTextureDefIndex) const;

	// Compute the hash key for the entire texture set (including all hashed textures)
	FString ComputeTextureSetDataKey() const;

	void CookImmediate(bool Force);

	UPROPERTY(Transient, DuplicateTransient)
	int32 CookedTexturesProcessedBitmask;

	UPROPERTY(EditAnywhere, Category="Debugging")
	FString AssetTag;

	// Replace FString by FGuid later for better performance, for now, keep FString for easy debugging as key is just plain text
	UPROPERTY(AdvancedDisplay, VisibleAnywhere)
	FString TextureSetDataKey;

	int GetNumPackedTextures() const { return PackedTextureData.Num(); }
	UTexture* GetPackedTexture(int Index) const { return PackedTextureData[Index].Texture; }

	void UpdateTextureData();
	void UpdateResource();

	const TMap<FName, FVector4> GetMaterialParameters();

private:

	void UpdateCookedTextures();

	UPROPERTY(AdvancedDisplay, VisibleAnywhere)
	TArray<FPackedTextureData> PackedTextureData;

};
