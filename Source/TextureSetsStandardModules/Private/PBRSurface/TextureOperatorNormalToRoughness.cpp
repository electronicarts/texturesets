// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "PBRSurface/TextureOperatorNormalToRoughness.h"

#include "PBRSurfaceModule.h"

void FTextureOperatorCombineVariance::ComputeGraphHash(FHashBuilder& HashBuilder) const
{
	FTextureOperator::ComputeGraphHash(HashBuilder);
	Normals->ComputeGraphHash(HashBuilder);
}

void FTextureOperatorCombineVariance::ComputeDataHash(const FTextureSetProcessingContext& Context, FHashBuilder& HashBuilder) const
{
	FTextureOperator::ComputeDataHash(Context, HashBuilder);
	Normals->ComputeDataHash(Context, HashBuilder);

	const UBPRNormalToRougnessParams* NormalToRougnessParams = Context.AssetParams.Get<UBPRNormalToRougnessParams>();
	HashBuilder << NormalToRougnessParams->bDisableNormalToRoughness;
}

void FTextureOperatorCombineVariance::Prepare(const FTextureSetProcessingContext& Context)
{
	if (bPrepared)
		return;

	FTextureOperator::Prepare(Context);
	Normals->Prepare(Context);

	const UBPRNormalToRougnessParams* NormalToRougnessParams = Context.AssetParams.Get<UBPRNormalToRougnessParams>();
	bDisableNormalToRoughness = NormalToRougnessParams->bDisableNormalToRoughness;

	RoughnessDef = SourceImage->GetTextureDef();
	RoughnessDim = SourceImage->GetTextureDimension();

	FTextureSetProcessedTextureDef NormalsDef = Normals->GetTextureDef();
	FTextureDimension NormalsDim = Normals->GetTextureDimension();

	check(RoughnessDef.ChannelCount == 1);
	check(NormalsDef.ChannelCount == 3);
	check(NormalsDef.Flags == RoughnessDef.Flags);
	check(RoughnessDim.Width == NormalsDim.Width);
	check(RoughnessDim.Height == NormalsDim.Height);
	check(RoughnessDim.Slices == NormalsDim.Slices);
	check(RoughnessDim.Mips == NormalsDim.Mips);

	bPrepared = true;
}

void FTextureOperatorCombineVariance::Cache()
{
	FTextureOperator::Cache();
	Normals->Cache();
}

void FTextureOperatorCombineVariance::WriteChannel(int32 Channel, int32 Mip, const FTextureDataTileDesc& Tile, float* TextureData) const
{	
	check(Channel == 0);

	// Write unmodified roughness into texture data
	SourceImage->WriteChannel(Channel, Mip, Tile, TextureData);

	if (!bDisableNormalToRoughness && Mip > 0) // Mip0 is assumed to have no variance
	{
		// Write mipped normals into a temporary buffer
		TArray<FVector3f> NormalData;
		NormalData.SetNumUninitialized(Tile.TileSize.X * Tile.TileSize.Y * Tile.TileSize.Z);

		for (int c = 0; c < 3; c++)
		{
			FTextureDataTileDesc NormalTile(
				Tile.TextureSize,
				Tile.TileSize,
				Tile.TileOffset,
				FTextureDataTileDesc::ComputeDataStrides(3, Tile.TileSize),
				c
			);

			Normals->WriteChannel(c, Mip, NormalTile, (float*)NormalData.GetData());
		}

		const FIntVector3 MipSize = TextureSetsHelpers::GetMipSize(RoughnessDim, Mip, RoughnessDef.IsVolume());
	
		Tile.ForEachPixel([TextureData, Mip, &Tile, &NormalData, &MipSize](FTextureDataTileDesc::ForEachPixelContext& Context)
		{
			const int32 NormalDataIndex = (Context.TileCoord.X % Tile.TileSize.X) + (Context.TileCoord.Y * Tile.TileSize.X);

			const FVector3f Normal = NormalData[NormalDataIndex] * 2.0f - 1.0f;
			float& Roughness = TextureData[Context.DataIndex];

			// Technique taken from Pg 21 of:
			// https://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf
			const float r = Normal.Length();
			float kappa = 0.0f;
			if(r < 1.0f)
				kappa = 1.0f / ((3 * r - r * r * r) / (1 - r * r));

			//Roughness = FMath::Sqrt((Roughness * Roughness) + kappa);
			//Roughness = Roughness + kappa;
			Roughness = FMath::Pow(FMath::Pow(Roughness, 4) + kappa, 0.25f);
		});
	}
}
