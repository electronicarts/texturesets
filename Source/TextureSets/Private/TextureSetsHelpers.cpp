// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureSetsHelpers.h"

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "AssetRegistry/AssetData.h"

FName TextureSetsHelpers::TextureBulkDataIdAssetTagName("TextureSet::TextureBulkDataId");

bool TextureSetsHelpers::GetSourceDataIdAsString(const UTexture* Texture, FString& StringOut)
{
	if (Texture->Source.IsValid() && !Texture->bSourceBulkDataTransient)
	{
		StringOut = Texture->Source.GetId().ToString();
		return true;
	}

	return false;
}

bool TextureSetsHelpers::GetSourceDataIdAsString(const FAssetData& AssetData, FString& StringOut)
{
	if (AssetData.IsValid())
	{
		const FAssetTagValueRef FoundValue = AssetData.TagsAndValues.FindTag(TextureBulkDataIdAssetTagName);
		if (FoundValue.IsSet())
		{
			StringOut = FoundValue.AsString();
			return true;
		}
	}

	return false;
}