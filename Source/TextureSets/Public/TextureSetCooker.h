// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetInfo.h"
#include "TextureSetModule.h"
#include "TextureSetDerivedData.h"
#if WITH_EDITOR
#include "DerivedDataPluginInterface.h"
#endif

class UTextureSet;

#if WITH_EDITOR

class TextureSetCooker : public FDerivedDataPluginInterface
{
public:

	TextureSetCooker(UTextureSet* TS, bool DefaultsOnly = false);

	bool IsOutOfDate() const;
	bool IsOutOfDate(int PackedTextureIndex) const;

	// FDerivedDataPluginInterface
	virtual const TCHAR* GetPluginName() const override;
	virtual const TCHAR* GetVersionString() const override;
	virtual FString GetPluginSpecificCacheKeySuffix() const override;
	virtual bool IsBuildThreadsafe() const override;
	virtual bool IsDeterministic() const override;
	virtual FString GetDebugContextString() const override;
	virtual bool Build(TArray<uint8>& OutData) override;

private:
	// Called for each packed texture of a texture set, can execute in parallel.
	void PackTexture(int Index, UTexture* Texture, FPackedTextureData& Data) const;

	UTextureSet* TextureSet;
	const UTextureSetDefinition* Definition;

	FTextureSetProcessingContext Context;
	
	const TextureSetDefinitionSharedInfo SharedInfo;
	const TextureSetPackingInfo PackingInfo;

	FString TextureSetDataKey;
	TArray<FString> PackedTextureKeys;
};
#endif