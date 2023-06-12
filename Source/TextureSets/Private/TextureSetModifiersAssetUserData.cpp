// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetModifiersAssetUserData.h"
#include "TextureSet.h"

void UTextureSetModifiersAssetUserData::PostLoad()
{
	Super::PostLoad();
}

void UTextureSetModifiersAssetUserData::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UTextureSetModifiersAssetUserData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

#if WITH_EDITOR
void UTextureSetModifiersAssetUserData::PostEditChangeOwner()
{
	Super::PostEditChangeOwner();
}
#endif // WITH_EDITOR

FString UTextureSetModifiersAssetUserData::GetIdString() const
{
	if (TextureSet != nullptr)
	{
		return TextureSet->GetPackedTextureDefKey(PackedTextureDefIndex);
	}

	return TEXT("INVALID_TEXTURE_SET");
}

void UTextureSetModifiersAssetUserData::ModifyTextureSource(UTexture* TextureAsset)
{
	Super::ModifyTextureSource(TextureAsset);

	if (TextureSet != nullptr)
	{
		TextureSet->ModifyTextureSource(PackedTextureDefIndex, TextureAsset);
	}
}

