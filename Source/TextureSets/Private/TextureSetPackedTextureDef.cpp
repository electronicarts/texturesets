// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetPackedTextureDef.h"

#include "TextureSetDefinition.h"

int FTextureSetPackedTextureDef::AvailableChannels() const
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
		return 0;
	}
}

int FTextureSetPackedTextureDef::UsedChannels() const
{
	int Specified = 0;

	if (!SourceA.IsNone())
		Specified = 4;
	else if (!SourceB.IsNone())
		Specified = 3;
	else if (!SourceG.IsNone())
		Specified = 2;
	else if (!SourceR.IsNone())
		Specified = 1;

	return FMath::Min(AvailableChannels(), Specified);
}

TArray<FName> FTextureSetPackedTextureDef::GetSources() const
{
	TArray<FName> ReturnValue;
	if (UsedChannels() >= 1)
		ReturnValue.Add(SourceR);
	if (UsedChannels() >= 2)
		ReturnValue.Add(SourceG);
	if (UsedChannels() >= 3)
		ReturnValue.Add(SourceB);
	if (UsedChannels() >= 4)
		ReturnValue.Add(SourceA);
	return ReturnValue;
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
		//SourceChannelString.ToLowerInline();

		if (RemoveDuplicate)
			SourcesWithoutChannel.AddUnique(SourceChannelString);
		else
			SourcesWithoutChannel.Add(SourceChannelString);
	}

	return SourcesWithoutChannel;
}

bool FTextureSetPackedTextureDef::GetHardwareSRGBEnabled() const
{
	TArray<TextureCompressionSettings> SRGBSupportedFormats =
	{
		TC_Default,
		TC_EditorIcon,
		TC_BC7,
		TC_LQ
	};

	return bHardwareSRGB && SRGBSupportedFormats.Contains(CompressionSettings);
}
