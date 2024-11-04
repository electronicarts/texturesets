// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetPackedTextureDef.h"

int FTextureSetPackedTextureDef::GetAvailableChannels() const
{
	switch (CompressionSettings)
	{
	case TC_Default:
	case TC_Masks:
	case TC_HDR:
	case TC_HDR_F32:
	case TC_EditorIcon:
	case TC_BC7:
		return 4;
	case TC_VectorDisplacementmap:
	case TC_HDR_Compressed:
	case TC_LQ:
		return 3;
	case TC_Normalmap:
		return 2;
	case TC_Displacementmap:
	case TC_Grayscale:
	case TC_Alpha:
	case TC_HalfFloat:
	case TC_DistanceFieldFont:
	case TC_SingleFloat:
		return 1;
	default:
		checkf(false, TEXT("Unsupported compression setting"));
		return 0;
	}
}

int FTextureSetPackedTextureDef::GetUsedChannels() const
{
	if (!SourceA.IsNone())
		return 4;
	else if (!SourceB.IsNone())
		return 3;
	else if (!SourceG.IsNone())
		return 2;
	else if (!SourceR.IsNone())
		return 1;
	else
		return 0;
}

TArray<FName> FTextureSetPackedTextureDef::GetSources() const
{
	TArray<FName> ReturnValue;
	const int Channels = FMath::Min(GetUsedChannels(), GetAvailableChannels());

	if (Channels >= 1)
		ReturnValue.Add(SourceR);
	if (Channels >= 2)
		ReturnValue.Add(SourceG);
	if (Channels >= 3)
		ReturnValue.Add(SourceB);
	if (Channels >= 4)
		ReturnValue.Add(SourceA);

	return ReturnValue;
}

void FTextureSetPackedTextureDef::SetSource(int Index, FName Value)
{
	switch (Index)
	{
	case 0:
		SourceR = Value;
		break;
	case 1:
		SourceG = Value;
		break;
	case 2:
		SourceB = Value;
		break;
	case 3:
		SourceA = Value;
		break;
	default:
		break;
	}
}

TArray<FString> FTextureSetPackedTextureDef::GetSourcesWithoutChannel(bool RemoveDuplicate) const
{
	TArray<FString> SourcesWithoutChannel;

	const TArray<FName>& PackedSources = GetSources();
	for (const FName& SourceName : PackedSources)
	{
		FString SourceChannelString = SourceName.GetPlainNameString();
		int ChannelNameStartPos = -1;
		if (SourceChannelString.FindLastChar('.', ChannelNameStartPos))
		{
			SourceChannelString.LeftInline(ChannelNameStartPos);
		}

		if (RemoveDuplicate)
			SourcesWithoutChannel.AddUnique(SourceChannelString);
		else
			SourcesWithoutChannel.Add(SourceChannelString);
	}

	return SourcesWithoutChannel;
}

bool FTextureSetPackedTextureDef::GetHardwareSRGBSupported() const
{
	static const TArray<TextureCompressionSettings> SRGBSupportedFormats =
	{
		TC_Default,
		TC_EditorIcon,
		TC_BC7,
		TC_LQ
	};

	return SRGBSupportedFormats.Contains(CompressionSettings);
}

#if WITH_EDITOR
void FTextureSetPackedTextureDef::UpdateAvailableChannels()
{
	AvailableChannels = GetAvailableChannels();
	
	// Clear any data in invalid channels
	switch (AvailableChannels)
	{
	case 0:
		SourceR = FName();
		// Falls through
	case 1:
		SourceG = FName();
		// Falls through
	case 2:
		SourceB = FName();
		// Falls through
	case 3:
		SourceA = FName();
		break;
	default:
		break;
	}
}
#endif
