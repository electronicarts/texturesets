// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TextureSetAssetParams.Generated.h"

// Data class which is instanced on each texture set asset. Module can use this to allow users to configure asset settings.
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UTextureSetAssetParams : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class UTextureSetSampleParams : public UTextureSetAssetParams
{
	GENERATED_BODY()
};

USTRUCT()
struct FTextureSetAssetParamsCollection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, EditFixedSize)
	TArray<class UTextureSetAssetParams*> ParamList;

	template <class T>
	const T* Get() const
	{
		for (UTextureSetAssetParams* Params : ParamList)
		{
			const T* P = Cast<T>(Params);

			if (IsValid(P))
			{
				return P;
			}
		}
		return GetDefault<T>(); // Not found, return the default class
	}

	void UpdateParamList(UObject* Outer, TArray<TSubclassOf<UTextureSetAssetParams>> RequiredParamClasses);

};
