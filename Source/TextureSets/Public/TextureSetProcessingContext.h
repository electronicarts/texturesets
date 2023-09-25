// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

struct FTextureSetProcessingContext
{
	FTextureSetProcessingContext(const class UTextureSet* TextureSet);

public:
	bool HasSourceTexure(FName Name) const { return SourceTextures.Contains(Name); }
	const TObjectPtr<class UTexture> GetSourceTexture(FName Name) const { return SourceTextures.FindChecked(Name).LoadSynchronous(); }
private:
	TMap<FName, TSoftObjectPtr<UTexture>> SourceTextures;
};
#endif
