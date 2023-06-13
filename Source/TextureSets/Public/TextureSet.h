// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SharedPointer.h"
#include "ImageCore.h"

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

	virtual void PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext) override;
	virtual void PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void FixupData();

	void UpdateCookedTextures();

	FString GetPackedTextureDefKey(int PackedTextureDefIndex);
	TArray<FString> ComputePackedTextureKeys();

	void ModifyTextureSource(int PackedTextureDefIndex, UTexture* TextureAsset);

	UPROPERTY(Transient, DuplicateTransient)
	bool IsTextureSetProcessed;
	UPROPERTY(Transient, DuplicateTransient)
	int32 CookedTexturesProcessedBitmask;

	UPROPERTY(EditAnywhere, Category="Debugging")
	FString AssetTag;

	// Replace FString by FGuid later for better performance, for now, keep FString for easy debugging as key is just plain text
	UPROPERTY()
	TArray<FString> PackedTextureKeys; 

	TMap<FName, TRefCountPtr<FSharedImage>> SourceRawImages;

	mutable FCriticalSection TextureSetCS;

	int GetNumCookedTextures() const { return CookedTextures.Num(); }
	UTexture* GetCookedTexture(int Index) const { return CookedTextures[Index].IsValid() ? CookedTextures[Index].Get() : CookedTextures[Index].LoadSynchronous(); }
	void UpdateResource();

private:
	UPROPERTY(AdvancedDisplay, EditAnywhere) // Temp EditAnywhere, for testing
	TArray<TSoftObjectPtr<UTexture>> CookedTextures;

	UPROPERTY(AdvancedDisplay, EditAnywhere) // Temp EditAnywhere, for testing
	TMap<FName, FVector4> ShaderParameters;
};
