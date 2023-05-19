//
// (c) Electronic Arts.  All Rights Reserved.
//

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "TextureSet.generated.h"


class UTexture;
class UTextureSetDefinition;

TObjectPtr<UTexture2D> LoadDefaultTexture();

UCLASS(BlueprintType, hidecategories = (Object))
class TEXTURESETS_API UTextureSet : public UObject
{
	GENERATED_UCLASS_BODY()
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

	UPROPERTY()
	TArray<UTexture*> CookedTextures;

	UPROPERTY()
	TMap<FName, FVector4> ShaderParameters;
	
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	void FixupData();

};
