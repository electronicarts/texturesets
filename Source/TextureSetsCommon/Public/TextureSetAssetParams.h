// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TextureSetAssetParams.generated.h"

// Data class which is instanced on each texture set asset. Module can use this to allow users to configure asset settings.
UCLASS(Abstract, Optional, EditInlineNew, DefaultToInstanced, CollapseCategories)
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
	#if WITH_EDITOR
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

	static FOnTextureSetAssetParamsCollectionChanged OnCollectionChangedDelegate;
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY(Instanced, EditAnywhere, Category="ParamCollection", EditFixedSize, NoClear)
	TArray<class UTextureSetAssetParams*> ParamList;
#endif
};
