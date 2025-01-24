// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "TextureSetCompiler.h"

#include "TextureSetTextureSourceProvider.generated.h"

UCLASS()
class TEXTURESETSCOMPILER_API UTextureSetTextureSourceProvider : public UTextureSourceProvider
{
	GENERATED_BODY()
public:
	UTextureSetTextureSourceProvider();

#if WITH_EDITOR
	TSharedPtr<const FTextureSetCompilerArgs> CompilerArgs;
	int Index;

	// UTextureSourceProvider Interface
	virtual void ConfigureTexture(UTexture* Texture) const override;
	virtual void Prepare(UTexture* Texture) override;
	virtual void UpdateSource(FTextureSource& Source) const override;
	virtual void CleanUp() override;

	FDerivedTexture& GetDerivedTexture() const;

private:
	TUniquePtr<FTextureSetCompiler> Compiler;
	bool bIsPrepared;
#endif
};