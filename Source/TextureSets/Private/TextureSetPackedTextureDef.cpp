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