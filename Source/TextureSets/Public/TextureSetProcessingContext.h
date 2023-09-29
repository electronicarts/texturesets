// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "TextureSetAssetParams.h"

struct FTextureSetProcessingContext
{
	FTextureSetProcessingContext(const class UTextureSet* TextureSet);

public:
	bool HasSourceTexure(FName Name) const { return SourceTextures.Contains(Name); }
	const TObjectPtr<class UTexture> GetSourceTexture(FName Name) const { return SourceTextures.FindChecked(Name); }

	template <class T> const T* GetAssetParam() const { return AssetParams.Get<T>(); }

private:
	TMap<FName, TObjectPtr<UTexture>> SourceTextures;
	FTextureSetAssetParamsCollection AssetParams;
};
#endif
