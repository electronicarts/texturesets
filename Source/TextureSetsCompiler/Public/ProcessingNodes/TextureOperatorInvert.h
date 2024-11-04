// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureOperator.h"

class FTextureOperatorInvert : public FTextureOperator
{
public:
	FTextureOperatorInvert(TSharedRef<ITextureProcessingNode> I) : FTextureOperator(I)
	{}

	virtual FName GetNodeTypeName() const  { return "Invert"; }

	virtual void ComputeChannel(const FTextureChannelDataDesc& Channel, float* TextureData) const override
	{
		SourceImage->ComputeChannel(Channel, TextureData);

		Channel.ForEachPixel([TextureData](int64 DataIndex)
		{
			TextureData[DataIndex] = 1.0f - TextureData[DataIndex];
		});
	}
};
