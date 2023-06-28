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

class TextureSetDerivedDataPlugin : public FDerivedDataPluginInterface
{
public:
	TextureSetDerivedDataPlugin(TSharedRef<TextureSetCooker> CookerRef);

	// FDerivedDataPluginInterface
	virtual const TCHAR* GetPluginName() const override;
	virtual const TCHAR* GetVersionString() const override;
	virtual FString GetPluginSpecificCacheKeySuffix() const override;
	virtual bool IsBuildThreadsafe() const override;
	virtual bool IsDeterministic() const override;
	virtual FString GetDebugContextString() const override;
	virtual bool Build(TArray<uint8>& OutData) override;

private:
	TSharedRef<TextureSetCooker> Cooker;
};

class TextureSetCooker
{
	friend class TextureSetDerivedDataPlugin;
public:

	TextureSetCooker(UTextureSet* TS);

	bool IsOutOfDate() const;
	bool IsOutOfDate(int PackedTextureIndex) const;

	void Build() const;
	// Called for each packed texture of a texture set, can execute in parallel.
	void BuildTextureData(int Index) const;

	void UpdateTexture(int Index) const;

private:

	UTextureSet* TextureSet;
	const UTextureSetDefinition* Definition;

	FTextureSetProcessingContext Context;
	
	const TextureSetDefinitionSharedInfo SharedInfo;
	const TextureSetPackingInfo PackingInfo;

	FString TextureSetDataKey;
	TArray<FString> PackedTextureKeys;
};
#endif