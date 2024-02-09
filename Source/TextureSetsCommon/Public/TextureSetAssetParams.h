// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TextureSetAssetParams.Generated.h"

// Data class which is instanced on each texture set asset. Module can use this to allow users to configure asset settings.
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class TEXTURESETSCOMMON_API UTextureSetAssetParams : public UObject
{
	GENERATED_BODY()
	
public:
	virtual void FixupData(UObject* Outer) {}
};

UCLASS()
class TEXTURESETSCOMMON_API UTextureSetSampleParams : public UTextureSetAssetParams
{
	GENERATED_BODY()
};

#if WITH_EDITOR
DECLARE_MULTICAST_DELEGATE(FOnTextureSetAssetParamsCollectionChanged);
#endif

USTRUCT()
struct TEXTURESETSCOMMON_API FTextureSetAssetParamsCollection
{
	GENERATED_BODY()

public:
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

#if WITH_EDITOR
	void UpdateParamList(UObject* Outer, TArray<TSubclassOf<UTextureSetAssetParams>> RequiredParamClasses);

	static FOnTextureSetAssetParamsCollectionChanged OnCollectionChangedDelegate;
#endif

private:
	UPROPERTY(Instanced, EditAnywhere, EditFixedSize, NoClear)
	TArray<class UTextureSetAssetParams*> ParamList;
};
