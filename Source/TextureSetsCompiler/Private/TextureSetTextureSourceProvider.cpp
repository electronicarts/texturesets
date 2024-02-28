// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureSetTextureSourceProvider.h"
#include "TextureSetsHelpers.h"

UTextureSetTextureSourceProvider::UTextureSetTextureSourceProvider() : Super()
	, Index (-1)
	, bIsPrepared(false)
{
}

void UTextureSetTextureSourceProvider::ConfigureTexture(UTexture* Texture) const
{
	// Called before checking if a texture needs to be built.
	// Configures the properties of the texture to use the correct DDC keys.

	if (!CompilerArgs.IsValid() || Index < 0)
	{
		// If compiler args are not valid, patch ourselves out of the texture
		UE_LOG(LogTextureSet, Log, TEXT("%s: UTextureSetTextureSourceProvider has not been properly configured. Removing from %s"), *this->GetName(), *Texture->GetName());
		Texture->ProceduralTextureProvider = nullptr;
		Texture->bSourceBulkDataTransient = false;
		
		if (!Texture->Source.IsValid())
		{
			uint8 BlackPixels[4*4] = { 0 };
			Texture->Source.Init(4, 4, 1, 1, TSF_G8, BlackPixels);
		}

		return;
	}

	check(IsInGameThread());
	check(Texture == GetOuter());

	FDerivedTexture& DerivedTexture = GetDerivedTexture();
	FScopeLock Lock(DerivedTexture.TextureCS.Get());
	check(DerivedTexture.Texture == Texture);
	check(DerivedTexture.Texture->ProceduralTextureProvider == this);

	FTextureSetCompiler TempCompiler(CompilerArgs.ToSharedRef());
	TempCompiler.ConfigureTexture(DerivedTexture, Index);
}

void UTextureSetTextureSourceProvider::Prepare(UTexture* Texture)
{
	// Called when we a texture build is imminent, and we need to prepare to update it's source.

	check(IsInGameThread());
	check(!bIsPrepared);
	check(!Compiler.IsValid());
	check(CompilerArgs.IsValid())

	FDerivedTexture& DerivedTexture = GetDerivedTexture();
	FScopeLock Lock(DerivedTexture.TextureCS.Get());
	check(DerivedTexture.Texture == Texture);

	Compiler = MakeUnique<FTextureSetCompiler>(CompilerArgs.ToSharedRef());
	Compiler->LoadResources();
	Compiler->InitializeTextureSource(DerivedTexture, Index);
	bIsPrepared = true;
}

void UTextureSetTextureSourceProvider::UpdateSource(FTextureSource& Source) const
{
	// Called on a worker thread just before building the derived texture's derived data.
	check(bIsPrepared);

	FDerivedTexture& DerivedTexture = GetDerivedTexture();
	FScopeLock Lock(DerivedTexture.TextureCS.Get());
	check(&DerivedTexture.Texture->Source == &Source);
	check(Compiler.IsValid());

	Compiler->GenerateTextureSource(DerivedTexture, Index);
}

void UTextureSetTextureSourceProvider::CleanUp()
{
	// Called on the main thread when the texture build has completed so we can deallocate memory.
	FDerivedTexture& DerivedTexture = GetDerivedTexture();
	FScopeLock Lock(DerivedTexture.TextureCS.Get());

	check(IsInGameThread());
	check(bIsPrepared);
	check(Compiler.IsValid());
	bIsPrepared = false;
	Compiler = nullptr;
}

FDerivedTexture& UTextureSetTextureSourceProvider::GetDerivedTexture() const
{
	UTexture* Texture = CastChecked<UTexture>(GetOuter());
	UTextureSetDerivedData* DerivedData = CastChecked<UTextureSetDerivedData>(Texture->GetOuter());
	check(Index >= 0);
	check(Index < DerivedData->Textures.Num());
	return DerivedData->Textures[Index];
}
