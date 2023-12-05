// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureSetTextureSourceProvider.h"

#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetProcessingGraph.h"
#include "ProcessingNodes/IProcessingNode.h"
#include "TextureSetCompilingManager.h"
#include "TextureSetCompiler.h"

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
		// Just fill it with some valid data until it gets removed
		float WhitePixel[1] = {1.0f};
		Source.Init(1, 1, 1, 1, TSF_R32F, (uint8*)WhitePixel);
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
