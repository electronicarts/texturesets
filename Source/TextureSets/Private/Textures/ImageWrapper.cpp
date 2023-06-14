// (c) Electronic Arts. All Rights Reserved.

#include "Textures/ImageWrapper.h"

FImageWrapper::FImageWrapper(const FImage& I, int Encoding)
{
	I.Linearize(Encoding, Image);

	switch (I.Format)
	{
	case ERawImageFormat::G8:
	case ERawImageFormat::G16:
	case ERawImageFormat::R16F:
	case ERawImageFormat::R32F:
		ValidChannels = 1;
		break;
	case ERawImageFormat::BGRE8:
	case ERawImageFormat::BGRA8:
	case ERawImageFormat::RGBA16:
	case ERawImageFormat::RGBA16F:
	case ERawImageFormat::RGBA32F:
		ValidChannels = 4;
		break;
	default:
		unimplemented();
	}
}

float FImageWrapper::GetPixel(int X, int Y, int Channel) const
{
	const int I = Y * Image.GetWidth() + X;
	return Image.AsRGBA32F()[I].Component(Channel);
}
