// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "ProcessingNodes/TextureOperatorEnlarge.h"

void FTextureOperatorEnlarge::WriteChannel(int32 Channel, const FTextureDataTileDesc& Tile, float* TextureData) const
{
	const FIntVector TargetSize = FIntVector(TargetWidth, TargetHeight, TargetSlices);
	const FTextureDimension SourceDimension = SourceImage->GetTextureDimension();
	const FTextureSetProcessedTextureDef SourceDef = GetTextureDef();
	const FIntVector SourceSize = FIntVector(SourceDimension.Width, SourceDimension.Height, SourceDimension.Slices);

	auto TransformToSource = [&TargetSize, &SourceSize](const FIntVector& Position)
	{
		FVector Ratio(
			(float)SourceSize.X / (float)TargetSize.X,
			(float)SourceSize.Y / (float)TargetSize.Y,
			(float)SourceSize.Z / (float)TargetSize.Z
		);

		return FIntVector(FVector(Position) * Ratio);
	};

	auto CalulateInterp = [](int32 TargetCoord, int32 TargetSize, int32 SourceSize)
	{
		return float((TargetCoord * SourceSize) % TargetSize) / float(TargetSize);
	};

	// Create a temp buffer to hold the tile of the image we need to enlarge
	FIntVector SourceTileOffset = TransformToSource(Tile.TileOffset);
	FIntVector SourceTileSize = (TransformToSource(Tile.TileSize) + FIntVector(1,1,1)).ComponentMin(SourceSize - SourceTileOffset);
		
	TArray64<float> SourceTextureData;
	SourceTextureData.SetNumUninitialized(SourceTileSize.X * SourceTileSize.Y * SourceTileSize.Z);

	FTextureDataTileDesc SourceTile = FTextureDataTileDesc(
		SourceSize,
		SourceTileSize,
		SourceTileOffset,
		FTextureDataTileDesc::ComputeDataStrides(1, SourceTileSize),
		0
	);
		
	SourceImage->WriteChannel(Channel, SourceTile, SourceTextureData.GetData());

	// Don't do trilinear filtering for 2d textures, or texture arrays.
	const bool bTrilinear = SourceTileSize.Z > 0 && !(SourceDef.Flags & (uint8)ETextureSetTextureFlags::Array);

	FTextureDataTileDesc::ForEachPixelContext Context;
	Context.DataIndex = Tile.TileDataOffset;
	FVector3f Lerp;

	if (bTrilinear)
	{
		for (Context.TileCoord.Z = 0; Context.TileCoord.Z < Tile.TileSize.Z; Context.TileCoord.Z++)
		{
			Lerp.Z = CalulateInterp(Context.TileCoord.Z + Tile.TileOffset.Z, TargetSize.Z, SourceSize.Z);

			for (Context.TileCoord.Y = 0; Context.TileCoord.Y < Tile.TileSize.Y; Context.TileCoord.Y++)
			{
				Lerp.Y = CalulateInterp(Context.TileCoord.Y + Tile.TileOffset.Y, TargetSize.Y, SourceSize.Y);

				for (Context.TileCoord.X = 0; Context.TileCoord.X < Tile.TileSize.X; Context.TileCoord.X++)
				{
					Lerp.X = CalulateInterp(Context.TileCoord.X + Tile.TileOffset.X, TargetSize.X, SourceSize.X);

					const FIntVector SourceBaseCoord = TransformToSource(Context.TileCoord + Tile.TileOffset) - SourceTile.TileOffset;
		
					float Values[2][2][2];
		
					// Initialize values
					for(int x = 0; x < 2; x++)
						for(int y = 0; y < 2; y++)
							for(int z = 0; z < 2; z++)
							{
								const FIntVector SourceCoord = (SourceBaseCoord + FIntVector(x,y,z)).ComponentMin(SourceTile.TileSize - FIntVector(1,1,1));
								Values[x][y][z] = SourceTextureData[SourceTile.TileCoordToDataIndex(SourceCoord)];
							}
		
					// Lerp in Z
					if (Lerp.Z > 0)
						for(int x = 0; x < 2; x++)
							for(int y = 0; y < 2; y++)
								Values[x][y][0] = FMath::Lerp(Values[x][y][0], Values[x][y][1], Lerp.Z);
		
					// Lerp in Y
					if (Lerp.Y > 0)
						for(int x = 0; x < 2; x++)
							Values[x][0][0] = FMath::Lerp(Values[x][0][0], Values[x][1][0], Lerp.Y);
		
					// Lerp in X
					if (Lerp.X > 0)
						Values[0][0][0] = FMath::Lerp(Values[0][0][0], Values[1][0][0], Lerp.X);
					
					TextureData[Context.DataIndex] = Values[0][0][0];

					Context.DataIndex += Tile.TileDataStepSize.X; // Next Pixel
				}
				Context.DataIndex += Tile.TileDataStepSize.Y; // Next row
			}
			Context.DataIndex += Tile.TileDataStepSize.Z; // Next slice
		}
	}
	else
	{
		for (Context.TileCoord.Z = 0; Context.TileCoord.Z < Tile.TileSize.Z; Context.TileCoord.Z++)
		{
			for (Context.TileCoord.Y = 0; Context.TileCoord.Y < Tile.TileSize.Y; Context.TileCoord.Y++)
			{
				Lerp.Y = CalulateInterp(Context.TileCoord.Y + Tile.TileOffset.Y, TargetSize.Y, SourceSize.Y);

				for (Context.TileCoord.X = 0; Context.TileCoord.X < Tile.TileSize.X; Context.TileCoord.X++)
				{
					Lerp.X = CalulateInterp(Context.TileCoord.X + Tile.TileOffset.X, TargetSize.X, SourceSize.X);

					const FIntVector SourceBaseCoord = TransformToSource(Context.TileCoord + Tile.TileOffset) - SourceTile.TileOffset;
		
					float Values[2][2];
		
					// Initialize values
					for(int x = 0; x < 2; x++)
						for(int y = 0; y < 2; y++)
						{
							const FIntVector SourceCoord = (SourceBaseCoord + FIntVector(x,y,0)).ComponentMin(SourceTile.TileSize - FIntVector(1,1,1));
							Values[x][y] = SourceTextureData[SourceTile.TileCoordToDataIndex(SourceCoord)];
						}
		
					// Lerp in Y
					if (Lerp.Y > 0)
						for(int x = 0; x < 2; x++)
							Values[x][0] = FMath::Lerp(Values[x][0], Values[x][1], Lerp.Y);
		
					// Lerp in X
					if (Lerp.X > 0)
						Values[0][0] = FMath::Lerp(Values[0][0], Values[1][0], Lerp.X);
					
					TextureData[Context.DataIndex] = Values[0][0];

					Context.DataIndex += Tile.TileDataStepSize.X; // Next Pixel
				}
				Context.DataIndex += Tile.TileDataStepSize.Y; // Next row
			}
			Context.DataIndex += Tile.TileDataStepSize.Z; // Next slice
		}
	}
}