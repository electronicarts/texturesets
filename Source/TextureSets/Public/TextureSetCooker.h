// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetInfo.h"
#include "TextureSetModule.h"

//#include "TextureSetCooker.generated.h"

class UTextureSet;

DECLARE_DELEGATE_OneParam(FOnTextureSetCookerReportProgress, float)

class TextureSetCooker
{
public:

	TextureSetCooker(UTextureSet* TS, FOnTextureSetCookerReportProgress Report = nullptr);

	// Called once on a texture-set before packing
	void Prepare();

	// Called for each packed texture of a texture set, can execute in parallel.
	void PackTexture(int Index, TMap<FName, FVector4>& MaterialParams) const;

	void PackAllTextures(TMap<FName, FVector4>& MaterialParams) const;

private:
	bool IsPrepared;

	const UTextureSet* TextureSet;
	const UTextureSetDefinition* Definition;

	const FOnTextureSetCookerReportProgress ReportProgressDelegate;
	float ProgressStepSize;

	FTextureSetProcessingContext Context;
	
	const TextureSetDefinitionSharedInfo SharedInfo;
	const TextureSetPackingInfo PackingInfo;

	void ReportProgress() const
	{
		ReportProgressDelegate.ExecuteIfBound(ProgressStepSize);
	}
};

