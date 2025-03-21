// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "ProcessingNodes/TextureOperatorLodBias.h"

#include "ProcessingNodes/TextureOperatorEnlarge.h"

void FTextureOperatorLodBias::ComputeGraphHash(FHashBuilder& HashBuilder) const
{
	FTextureOperator::ComputeGraphHash(HashBuilder);
	HashBuilder << MipBias;

	// Use a static FTextureOperatorEnlarge to determine if the graph hash of the enlarge op changes
	const static FTextureOperatorEnlarge EnlargeOperator(SourceImage, 1, 1, 1);
	EnlargeOperator.ComputeGraphHash(HashBuilder);
}

void FTextureOperatorLodBias::Prepare(const FTextureSetProcessingContext& Context)
{
	SourceImage->Prepare(Context);

	if (MipBias < 0) // Mip bias is negative, so we enlarge
	{
		const int32 MipsToAdd = -MipBias;

		const FTextureDimension SourceDim = SourceImage->GetTextureDimension();
		bool IsVolume = SourceImage->GetTextureDef().IsVolume();

		int32 Width = SourceDim.Width;
		int32 Height = SourceDim.Height;
		int32 Slices = SourceDim.Slices;

		EnlargeOperators.Reserve(MipsToAdd);

		for (int i = 0; i < MipsToAdd; i++)
		{
			Width *= 2;
			Height *= 2;

			if (IsVolume)
				Slices *= 2;

			EnlargeOperators.Add(MakeShared<FTextureOperatorEnlarge>(SourceImage, Width, Height, Slices));
		}
	}

	for (int i = 0; i < EnlargeOperators.Num(); i++)
		EnlargeOperators[i]->Prepare(Context);
}

void FTextureOperatorLodBias::Cache()
{
	SourceImage->Cache();

	for (int i = 0; i < EnlargeOperators.Num(); i++)
		EnlargeOperators[i]->Cache();
}

void FTextureOperatorLodBias::WriteChannel(int32 Channel, int32 Mip, const FTextureDataTileDesc& Tile, float* TextureData) const
{
	int32 SourceMip = Mip + MipBias;

	if (SourceMip >= 0) // Mip exists in source
	{
		SourceImage->WriteChannel(Channel, SourceMip, Tile, TextureData);
	}
	else // Source mip is negative, so we need to enlarge
	{
		int32 i = -SourceMip - 1;

		EnlargeOperators[i]->WriteChannel(Channel, 0, Tile, TextureData);
	}
}
