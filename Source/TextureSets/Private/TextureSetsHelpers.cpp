// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetsHelpers.h"

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "TextureSetPackedTextureDef.h"
#include "TextureSetInfo.h"
#include "AssetRegistry/AssetData.h"

const TArray<FString> TextureSetsHelpers::ChannelSuffixes = {".r", ".g", ".b", ".a"};

FName TextureSetsHelpers::TextureBulkDataIdAssetTagName("TextureSet::TextureBulkDataId");

#if WITH_EDITOR
bool TextureSetsHelpers::GetSourceDataIdAsString(const UTexture* Texture, FString& StringOut)
{
	if (Texture->Source.IsValid() && !Texture->bSourceBulkDataTransient)
	{
		StringOut = Texture->Source.GetId().ToString();
		return true;
	}

	return false;
}
#endif

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

TArray<FName> TextureSetsHelpers::GetUnpackedChannelNames(const TArray<FTextureSetPackedTextureDef>& PackedTextures, const TMap<FName, FTextureSetProcessedTextureDef>& ProcessedTextures)
{
	// Construct an array of all the channel names already used for packing
	TArray<FName> PackedNames = TArray<FName>();

	for (const FTextureSetPackedTextureDef& Tex : PackedTextures)
	{
		if (!Tex.SourceR.IsNone())
			PackedNames.Add(Tex.SourceR);

		if (!Tex.SourceG.IsNone())
			PackedNames.Add(Tex.SourceG);

		if (!Tex.SourceB.IsNone())
			PackedNames.Add(Tex.SourceB);

		if (!Tex.SourceA.IsNone())
			PackedNames.Add(Tex.SourceA);
	}

	// Construct a list of channel names not yet used for packing (remaining choices)
	TArray<FName> UnpackedNames = TArray<FName>{  FName() };

	for (const auto& [Name, Tex] : ProcessedTextures)
	{
		for (int i = 0; i < Tex.ChannelCount; i++)
		{
			FName ChannelName = FName(Name.ToString() + TextureSetsHelpers::ChannelSuffixes[i]);

			if (!PackedNames.Contains(ChannelName))
				UnpackedNames.Add(ChannelName);
		}
	}

	return UnpackedNames;
}

UReferenceHolder* FReferenceHolder::GetHolder()
{
	static UReferenceHolder* Holder = nullptr;

	if (!Holder)
	{
		Holder = NewObject<UReferenceHolder>();
		Holder->AddToRoot();
	}

	return Holder;
}
