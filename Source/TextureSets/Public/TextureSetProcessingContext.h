// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FTextureSetProcessingContext
{
	FTextureSetProcessingContext(const class UTextureSet* TextureSet);

public:
	bool HasSourceTexure(FName Name) const { return SourceTextures.Contains(Name); }
	const TObjectPtr<class UTexture> GetSourceTexture(FName Name) const { return SourceTextures.FindChecked(Name); }
private:
	TMap<FName, TObjectPtr<UTexture>> SourceTextures;
};