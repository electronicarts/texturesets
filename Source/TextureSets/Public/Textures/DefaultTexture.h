// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ITextureSetTexture.h"

class FDefaultTexture : public ITextureSetTexture
{
public:
	FDefaultTexture(FVector4 Col, int Chan)
		: Color(Col)
		, Channels(Chan)
	{}

	virtual int GetWidth() const override { return 4; }
	virtual int GetHeight() const override { return 4; }
	virtual int GetChannels() const override{ return Channels; }

	virtual float GetPixel(int X, int Y, int Channel) const override
	{
		check(Channel < Channels);
		return Color[Channel];
	}

private:
	FVector4 Color;
	int Channels;
};
