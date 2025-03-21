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

	virtual void WriteChannel(int32 Channel, int32 Mip, const FTextureDataTileDesc& Tile, float* TextureData) const override
	{
		SourceImage->WriteChannel(Channel, Mip, Tile, TextureData);

		Tile.ForEachPixel([TextureData](FTextureDataTileDesc::ForEachPixelContext& Context)
		{
			TextureData[Context.DataIndex] = 1.0f - TextureData[Context.DataIndex];
		});
	}
};
