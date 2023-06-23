// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetInfo.h"
#include "TextureSetModule.h"
#include "TextureSetDerivedData.h"

//#include "TextureSetCooker.generated.h"

class UTextureSet;

class TextureSetCooker
{
public:

	TextureSetCooker(UTextureSet* TS, bool DefaultsOnly = false);

	// Called for each packed texture of a texture set, can execute in parallel.
	void PackTexture(int Index, FPackedTextureData& Data) const;

	bool IsOutOfDate() const;
	bool IsOutOfDate(int PackedTextureIndex) const;

private:

	const UTextureSet* TextureSet;
	const UTextureSetDefinition* Definition;

	FTextureSetProcessingContext Context;
	
	const TextureSetDefinitionSharedInfo SharedInfo;
	const TextureSetPackingInfo PackingInfo;

	FString TextureSetDataKey;
	TArray<FString> PackedTextureKeys;
};

