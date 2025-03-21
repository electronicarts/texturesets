// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureOperator.h"

class FTextureOperatorLodBias : public FTextureOperator
{
public:
	FTextureOperatorLodBias(TSharedRef<ITextureProcessingNode> I, int32 MipBias) : FTextureOperator(I)
		, MipBias(MipBias)
	{
		Scale = 1.0f / FMath::Pow(2.0f, MipBias);
	}

	virtual FName GetNodeTypeName() const  { return "Lod Bias v1"; }

	virtual void ComputeGraphHash(FHashBuilder& HashBuilder) const override;

	virtual FTextureDimension GetTextureDimension() const override
	{
		const FTextureDimension SourceDim = SourceImage->GetTextureDimension();

		FTextureDimension OurDim;
		OurDim.Width = (float)SourceDim.Width * Scale;
		OurDim.Height = (float)SourceDim.Height * Scale;

		OurDim.Slices = SourceImage->GetTextureDef().IsVolume()
			? (float)SourceDim.Slices * Scale
			: SourceDim.Slices;

		OurDim.Mips = FMath::Max(1, SourceDim.Mips - MipBias);
		return OurDim;
	}

	virtual void Prepare(const FTextureSetProcessingContext& Context) override;

	virtual void Cache() override;

	virtual void WriteChannel(int32 Channel, int32 Mip, const FTextureDataTileDesc& Tile, float* TextureData) const override;

private:
	int32 MipBias;
	float Scale;
	TArray<TSharedPtr<class FTextureOperatorEnlarge>> EnlargeOperators;
};
