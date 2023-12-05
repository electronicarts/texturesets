// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture.h"

#include "TextureSetTextureSourceProvider.generated.h"

class UTextureSet;
class FTextureSetCompiler;

UCLASS()
class TEXTURESETS_API UTextureSetTextureSourceProvider : public UProceduralTextureProvider
{
	GENERATED_BODY()
public:
	UTextureSetTextureSourceProvider();
#if WITH_EDITOR

	// UProceduralTextureProvider Interface
	virtual void ConfigureTexture(UTexture* Texture) const override;
	virtual void Prepare(UTexture* Texture) override;
	virtual void UpdateSource(FTextureSource& Source) const override;
	virtual void CleanUp() override;

	static UTextureSet* GetTextureSetAndIndex(UTexture* Texture, int& OutIndex);

private:
	mutable FCriticalSection SourceCS;
	bool bIsPrepared;
	UTextureSet* TextureSet;
	int Index;
	TSharedPtr<FTextureSetCompiler> Compiler;
	mutable FGuid LastUpdateID;
#endif
};