// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "TextureSetAssetParams.h"
#include "TextureSetSourceTextureReference.h"

struct FTextureSetProcessingContext
{
	FTextureSetProcessingContext(const class UTextureSet* TextureSet);

public:
	bool HasSourceTexure(FName Name) const { return SourceTextures.Contains(Name); }
	const FTextureSetSourceTextureReference& GetSourceTexture(FName Name) const { return SourceTextures.FindChecked(Name); }

	template <class T> const T* GetAssetParam() const { return AssetParams.Get<T>(); }

private:
	TMap<FName, FTextureSetSourceTextureReference> SourceTextures;
	FTextureSetAssetParamsCollection AssetParams;
};
#endif
