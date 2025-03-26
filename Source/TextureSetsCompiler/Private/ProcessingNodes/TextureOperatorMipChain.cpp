// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "ProcessingNodes/TextureOperatorMipChain.h"

#include "TextureSetInfo.h"
#include "ImageCoreUtils.h"

ITextureProcessingNode::FTextureDimension FTextureOperatorMipChain::GetTextureDimension() const
{
	const FTextureDimension SourceDim = SourceImage->GetTextureDimension();

	FTextureDimension Dim;
	Dim.Width = SourceDim.Width;
	Dim.Height = SourceDim.Height;
	Dim.Slices = SourceDim.Slices;
	Dim.Mips = TextureSetsHelpers::GetMipCount(SourceDim, SourceImage->GetTextureDef().IsVolume());
	return Dim;
}

void FTextureOperatorMipChain::Prepare(const FTextureSetProcessingContext& Context)
{
	if (bPrepared)
		return;
	
	SourceImage->Prepare(Context);

	SourceDef = SourceImage->GetTextureDef();
	SourceDimension = SourceImage->GetTextureDimension();
	OurDim = GetTextureDimension();

	check(SourceDimension.Mips > 0);
	FirstCachedMip = SourceDimension.Mips;
	
	NumMipsToCache = FMath::Max(0, OurDim.Mips - SourceDimension.Mips);
	CachedMips.SetNum(OurDim.Mips);

	bPrepared = true;
}

static void ComputeMip(
	FIntVector3 SourceSize, float* SourceData,
	FIntVector3 DestSize, float* DestData,
	uint8 Channels, bool bIsVolume)
{
	check(SourceSize.X / 2 == DestSize.X)
	check(SourceSize.Y / 2 == DestSize.Y)
	if (bIsVolume)
		check(SourceSize.Z / 2 == DestSize.Z)
	else
		check(SourceSize.Z == DestSize.Z)

	const int PixelCount = DestSize.X * DestSize.Y * DestSize.Z;
	const int SourceLineStride = SourceSize.X * Channels;

	int DestIdx = 0;
	int SourceIdx[4];

	SourceIdx[0] = 0;
	SourceIdx[1] = Channels;
	SourceIdx[2] = SourceLineStride;
	SourceIdx[3] = SourceLineStride + Channels;
	
	if(bIsVolume)
	{
		// Not supported yet! Will need to average 8 values.
		unimplemented();
	}
	else
	{
		for (int z = 0; z < DestSize.Z; z++)
		{
			for (int y = 0; y < DestSize.Y; y++)
			{
				for (int x = 0; x < DestSize.X; x++)
				{
					for (int c = 0; c < Channels; c++)
					{
						// Simple average of the 4 values in the larger mip
						DestData[DestIdx] = 0.25f * (SourceData[SourceIdx[0]] + SourceData[SourceIdx[1]] + SourceData[SourceIdx[2]] + SourceData[SourceIdx[3]]);
						
						DestIdx++;
						SourceIdx[0]++;
						SourceIdx[1]++;
						SourceIdx[2]++;
						SourceIdx[3]++;
					}

					SourceIdx[0] += Channels;
					SourceIdx[1] += Channels;
					SourceIdx[2] += Channels;
					SourceIdx[3] += Channels;
				}

				SourceIdx[0] += SourceLineStride;
				SourceIdx[1] += SourceLineStride;
				SourceIdx[2] += SourceLineStride;
				SourceIdx[3] += SourceLineStride;
			}
		}
	}
}

void FTextureOperatorMipChain::Cache()
{
	FScopeLock Lock(&CacheCS);

	if (bCached)
		return;

	FTextureOperator::Cache();

	if (NumMipsToCache > 0)
	{
		// Cache the first mip by having the source node write to a buffer, and read from that.
		// Allocate tiles
		{
			check(FirstCachedMip > 0);
		
			// Allocate buffer for first mip data
			const FIntVector3 DestMipSize = TextureSetsHelpers::GetMipSize(SourceDimension, FirstCachedMip, SourceDef.IsVolume());
			CachedMips[FirstCachedMip].SetNum(DestMipSize.X * DestMipSize.Y * DestMipSize.Z * SourceDef.ChannelCount);

			// Allocate a temporary buffer for the source mip
			const FIntVector3 SourceMipSize = TextureSetsHelpers::GetMipSize(SourceDimension, FirstCachedMip - 1, SourceDef.IsVolume());
			TArray<float> SourceMipBuffer;
			SourceMipBuffer.SetNumUninitialized(SourceMipSize.X * SourceMipSize.Y * SourceMipSize.Z * SourceDef.ChannelCount);

			for (int c = 0; c < SourceDef.ChannelCount; c++)
			{
				FTextureDataTileDesc Tile(
					SourceMipSize,
					SourceMipSize,
					FIntVector3::ZeroValue,
					FTextureDataTileDesc::ComputeDataStrides(SourceDef.ChannelCount, SourceMipSize),
					c
				);

				SourceImage->WriteChannel(c, FirstCachedMip, Tile, SourceMipBuffer.GetData());
			}

			ComputeMip(
				SourceMipSize, SourceMipBuffer.GetData(),
				DestMipSize, CachedMips[FirstCachedMip].GetData(),
				SourceDef.ChannelCount, SourceDef.IsVolume());
		}

		// Subsequent mips can be computed by reading from previous mip caches
		for (int m = FirstCachedMip + 1; m < FirstCachedMip + NumMipsToCache; m++)
		{
			const FIntVector3 SourceMipSize = TextureSetsHelpers::GetMipSize(SourceDimension, m - 1, SourceDef.IsVolume());
			const FIntVector3 DestMipSize = TextureSetsHelpers::GetMipSize(SourceDimension, m, SourceDef.IsVolume());

			CachedMips[m].SetNum(DestMipSize.X * DestMipSize.Y * DestMipSize.Z * SourceDef.ChannelCount);

			ComputeMip(
				SourceMipSize, CachedMips[m-1].GetData(),
				DestMipSize, CachedMips[m].GetData(),
				SourceDef.ChannelCount, SourceDef.IsVolume());
		}
	}

	bCached = true;
}

void FTextureOperatorMipChain::WriteChannel(int32 Channel, int32 Mip, const FTextureDataTileDesc& Tile, float* TextureData) const
{
	check(bCached);

	if (Mip < SourceDimension.Mips)
	{
		// Source image already has this mip, so write directly from there
		SourceImage->WriteChannel(Channel, Mip, Tile, TextureData);
	}
	else
	{
		// Write from the cached mip
		check(CachedMips[Mip].Num() != 0);
		const float* MipData = CachedMips[Mip].GetData();
		const FIntVector3 MipSize = TextureSetsHelpers::GetMipSize(SourceDimension, Mip, SourceDef.IsVolume());
		const int MipChannelCount = SourceDef.ChannelCount;

		Tile.ForEachPixel([TextureData, Channel, &Tile, &MipData, &MipSize, &MipChannelCount](FTextureDataTileDesc::ForEachPixelContext& Context)
		{
			const FIntVector3 TextureCoord = Tile.TileOffset + Context.TileCoord;
			const int32 PixelIndex = (TextureCoord.X % MipSize.X) + (TextureCoord.Y * MipSize.Y);
			const int32 TextureDataIndex = (PixelIndex * MipChannelCount) + Channel;
			TextureData[Context.DataIndex] = MipData[TextureDataIndex];
		});
	}
}
