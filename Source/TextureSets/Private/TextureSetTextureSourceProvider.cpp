// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureSetTextureSourceProvider.h"

#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetProcessingGraph.h"
#include "ProcessingNodes/IProcessingNode.h"
#include "TextureSetCompilingManager.h"
#include "TextureSetCooker.h"

UTextureSetTextureSourceProvider::UTextureSetTextureSourceProvider() : Super()
#if WITH_EDITOR
	, bIsPrepared(false)
	, TextureSet(nullptr)
	, Index (-1)
	, Cooker(nullptr)
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
		TSharedPtr<FTextureSetCooker> TmpCooker = FTextureSetCompilingManager::Get().BorrowCooker(TmpTextureSet);

		TmpCooker->ConfigureTexture(TmpIndex);

		if (!bIsPrepared)
			TmpCooker->InitializeTextureData(TmpIndex);

		FTextureSetCompilingManager::Get().ReturnCooker(TmpTextureSet);
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
		Cooker = FTextureSetCompilingManager::Get().BorrowCooker(TextureSet);
		check(Cooker);

		if (LastUpdateID != Cooker->GetDerivedTextureID(Index))
		{
			// Only update source if Texture ID has changed since last time we were called
			bIsPrepared = true;
		}
		else
		{
			FTextureSetCompilingManager::Get().ReturnCooker(TextureSet);
			Cooker.Reset();
		}
	}
}

void UTextureSetTextureSourceProvider::UpdateSource(FTextureSource& Source) const
{
	FScopeLock Lock(&SourceCS);

	if (bIsPrepared)
	{
		check(Cooker);
		check(Index != -1);
		check(Cooker->GetDerivedData().Textures[Index].Get() == GetOuter()); // Sanity check
		Cooker->BuildTextureData(Index);
		LastUpdateID = Cooker->GetDerivedTextureID(Index);
	}
}

void UTextureSetTextureSourceProvider::CleanUp()
{
	FScopeLock Lock(&SourceCS);
	check(IsInGameThread());

	if (bIsPrepared)
	{
		// Reset everything and return the cooker
		check(Cooker);
		check(TextureSet);

		FTextureSetCompilingManager::Get().ReturnCooker(TextureSet);
		Cooker.Reset();

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

	if (!ensureMsgf(OutIndex != -1, TEXT("Derived texture not found in texture set's derived data!")))
		return nullptr;

	return TextureSet;
}
#endif
