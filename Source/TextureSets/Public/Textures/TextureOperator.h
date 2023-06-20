// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ITextureSetTexture.h"

class FTextureOperator : public ITextureSetTexture
{
public:
	FTextureOperator(const TSharedRef<ITextureSetTexture> I)
		: SourceImage(I)
	{}

	virtual void Initialize() { SourceImage->Initialize(); }

	virtual int GetWidth() const override { return SourceImage->GetWidth(); }
	virtual int GetHeight() const override { return SourceImage->GetHeight(); }
	virtual int GetChannels() const override { return SourceImage->GetChannels(); }

	const TSharedRef<ITextureSetTexture> SourceImage;
};
