// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureSetTextureSourceProvider.h"

#include "TextureSet.h"
#include "TextureSetCompilingManager.h"
#if WITH_EDITOR
#include "TextureSetCompiler.h"
#endif

UTextureSetTextureSourceProvider::UTextureSetTextureSourceProvider() : Super()
#if WITH_EDITOR
	, bIsPrepared(false)
	, TextureSet(nullptr)
	, Index (-1)
	, Compiler(nullptr)
	, LastUpdateID()
#endif
{
}

#if WITH_EDITOR
void UTextureSetTextureSourceProvider::ConfigureTexture(UTexture* Texture) const
{
	FScopeLock Lock(&SourceCS);
	check(IsInGameThread());
	check(Texture == GetOuter()); // UTextureSetTextureSourceProvider should always be under a texture

	int TmpIndex;
	UTextureSet* TmpTextureSet = GetTextureSetAndIndex(Texture, TmpIndex);

	if (IsValid(TmpTextureSet))
	{
		TSharedPtr<FTextureSetCompiler> TmpCompiler = FTextureSetCompilingManager::Get().BorrowCompiler(TmpTextureSet);

		TmpCompiler->ConfigureTexture(TmpIndex);

		if (!bIsPrepared)
			TmpCompiler->InitializeTextureData(TmpIndex);

		FTextureSetCompilingManager::Get().ReturnCompiler(TmpTextureSet);
	}
	else
	{
		// Can happen if a texture is orphaned but still resides in the TS package
		// and is loaded. In this case, ensure the placeholder source is at least
		// 4x4 to gracefully handle the case this texture was configured BC-compressed
		ensure(!bIsPrepared);
		UpdateSource(Texture->Source);
	}
}

void UTextureSetTextureSourceProvider::Prepare(UTexture* Texture)
{
	FScopeLock Lock(&SourceCS);
	check(IsInGameThread());
	check(Texture == GetOuter());

	TextureSet = GetTextureSetAndIndex(Texture, Index);

	if (!bIsPrepared && IsValid(TextureSet))
	{
		Compiler = FTextureSetCompilingManager::Get().BorrowCompiler(TextureSet);
		check(Compiler);

		if (LastUpdateID != Compiler->GetDerivedTextureID(Index))
		{
			// Only update source if Texture ID has changed since last time we were called
			bIsPrepared = true;
		}
		else
		{
			FTextureSetCompilingManager::Get().ReturnCompiler(TextureSet);
			Compiler.Reset();
		}
	}
}

void UTextureSetTextureSourceProvider::UpdateSource(FTextureSource& Source) const
{
	FScopeLock Lock(&SourceCS);

	if (bIsPrepared)
	{
		check(Compiler);
		check(Index != -1);
		check(Compiler->GetDerivedData().Textures[Index].Get() == GetOuter()); // Sanity check
		Compiler->BuildTextureData(Index);
		LastUpdateID = Compiler->GetDerivedTextureID(Index);
	}
	else if (!Source.IsValid())
	{
		// Can happen when a texture is removed from a definition
		// Just fill it with valid data and dims matching that in
		// FTextureSetCompiler::InitializeTextureData() until it gets removed
		constexpr float BlackPixels[16] = { 0.0f };
		Source.Init(4, 4, 1, 1, TSF_R32F, reinterpret_cast<const uint8*>(BlackPixels));
	}
}

void UTextureSetTextureSourceProvider::CleanUp()
{
	FScopeLock Lock(&SourceCS);
	check(IsInGameThread());

	if (bIsPrepared)
	{
		// Reset everything and return the compiler
		check(Compiler);
		check(TextureSet);

		FTextureSetCompilingManager::Get().ReturnCompiler(TextureSet);
		Compiler.Reset();

		Index = -1;
		TextureSet = nullptr;

		bIsPrepared = false;
	}
}

UTextureSet* UTextureSetTextureSourceProvider::GetTextureSetAndIndex(UTexture* Texture, int& OutIndex)
{
	UTextureSet* TextureSet = Cast<UTextureSet>(Texture->GetOuter());

	if (!ensureMsgf(TextureSet, TEXT("A texture set derived texture must be a child of the owning texture set")))
		return nullptr;

	const FTextureSetDerivedData& DerivedData = TextureSet->GetDerivedData();

	OutIndex = -1;
	for (int i = 0; i < DerivedData.Textures.Num(); i++)
	{
		if (DerivedData.Textures[i] == Texture)
		{
			OutIndex = i;
			break;
		}
	}

	if (OutIndex == -1)
		return nullptr; // Not found (happens when a packed texture is removed from a definition)
	else
		return TextureSet;
}
#endif
