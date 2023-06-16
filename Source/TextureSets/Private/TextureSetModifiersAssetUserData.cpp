// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetModifiersAssetUserData.h"
#include "TextureSet.h"
#include "UObject/ObjectSaveContext.h"

void UTextureSetModifiersAssetUserData::PreSave(FObjectPreSaveContext SaveContext) 
{
	Super::PreSave(SaveContext);

	UTexture* TextureOwner = Cast<UTexture>(GetOuter());
	if (TextureOwner != nullptr)
	{
		const int CookedTextureSize = 4;
		TArray<FColor> DummyImageData;

		for (int i = 0; i < (CookedTextureSize * CookedTextureSize); i++)
			DummyImageData.Add(FColor(0xCCCCCCFF));

		int32 NewSizeX = CookedTextureSize;
		int32 NewSizeY = CookedTextureSize;
		int32 NewNumSlices = 1;
		int32 NewNumLayers = 1;
		int32 NewNumMips = 1;
		ETextureSourceFormat NewLayerFormat = ETextureSourceFormat::TSF_BGRA8;

		// Replace the Source raw data by dummy 4x4 image
		// The source raw data is generated at loading time when needed, and saved into the Data derived cache
		// Raw data in the uasset file on the disk is not being used in any way, so replace it with dummy image to reduce the size on disk
		TextureOwner->Source.InitLayered(NewSizeX, NewSizeY, NewNumSlices, NewNumLayers, NewNumMips, &NewLayerFormat, (uint8*)DummyImageData.GetData());
		TextureOwner->Source.SetId(TextureSet->GetPackedTextureSourceGuid(), false);
	}
}

void UTextureSetModifiersAssetUserData::PostLoad()
{
	Super::PostLoad();

	// TODO_YICWANG:
	// AssetUserData::PostLoad is called after the owner Texture::PostLoad is called.
	// We need to inject the data hash before Texture::PostLoad. 
#if 0
	UTexture* TextureOwner = Cast<UTexture>(GetOuter());
	if ((TextureOwner != nullptr) && (TextureSet != nullptr))
	{
		FString PackedTextureDefKey = TextureSet->GetPackedTextureDefKey(PackedTextureDefIndex);

		// update Source Guid
		const FString& DDCKey = PackedTextureDefKey;
		const uint32 KeyLength = DDCKey.Len() * sizeof(DDCKey[0]);
		uint32 Hash[5];
		FSHA1::HashBuffer(*DDCKey, KeyLength, reinterpret_cast<uint8*>(Hash));
		FGuid SourceGuid = FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);

		TextureOwner->Source.SetId(SourceGuid, false);
	}
#endif
}

FString UTextureSetModifiersAssetUserData::GetIdString() const
{
	if (!IsValid(TextureSet))
		return TEXT("INVALID_TEXTURE_SET");

	return TextureSet->ComputePackedTextureKey(PackedTextureDefIndex);
}

void UTextureSetModifiersAssetUserData::ModifyTextureSource(UTexture* TextureAsset)
{
	Super::ModifyTextureSource(TextureAsset);

	if (TextureSet != nullptr)
	{
		TextureSet->ModifyTextureSource(PackedTextureDefIndex, TextureAsset);
	}
}

