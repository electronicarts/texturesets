// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "TextureOperator.h"
#include "TextureSetProcessingContext.h"
#include "TextureSetsHelpers.h"

class TEXTURESETSCOMPILER_API FTextureOperatorLodBias : public FTextureOperator
{
public:
	// Apply a fixed lod bias
	FTextureOperatorLodBias(TSharedRef<ITextureProcessingNode> I, int32 MipBias) : FTextureOperator(I)
		, MipBias(MipBias)
		, Reference(I)
		, bPrepared(false)
	{
	}

	// Apply a lod bias to match the size of another texture
	FTextureOperatorLodBias(TSharedRef<ITextureProcessingNode> I, TSharedRef<ITextureProcessingNode> R) : FTextureOperator(I)
		, MipBias(0)
		, Reference(R)
		, bPrepared(false)
	{
	}

	virtual FName GetNodeTypeName() const  { return "Lod Bias v1"; }

	virtual void ComputeGraphHash(FHashBuilder& HashBuilder) const override;
	virtual void ComputeDataHash(const FTextureSetProcessingContext& Context, FHashBuilder& HashBuilder) const override;
	virtual void Prepare(const FTextureSetProcessingContext& Context) override;
	virtual void Cache() override;
	virtual void WriteChannel(int32 Channel, int32 Mip, const FTextureDataTileDesc& Tile, float* TextureData) const override;

	virtual FTextureDimension GetTextureDimension() const override
	{
		check(bPrepared);
		const FTextureDimension SourceDim = SourceImage->GetTextureDimension();

		FIntVector3 Size = TextureSetsHelpers::GetMipSize(Reference->GetTextureDimension(), TotalMipBias, SourceImage->GetTextureDef().IsVolume());
		float Scale = 1.0f / FMath::Pow(2.0f, TotalMipBias);

		FTextureDimension OurDim;
		OurDim.Width = (float)SourceDim.Width * Scale;
		OurDim.Height = (float)SourceDim.Height * Scale;

		OurDim.Slices = SourceImage->GetTextureDef().IsVolume()
			? (float)SourceDim.Slices * Scale
			: SourceDim.Slices;

		OurDim.Mips = FMath::Max(1, SourceDim.Mips - TotalMipBias);
		return OurDim;
	}

private:
	const int32 MipBias;
	const TSharedPtr<ITextureProcessingNode> Reference;

	int32 TotalMipBias;
	bool bPrepared;
	TArray<TSharedPtr<class FTextureOperatorEnlarge>> EnlargeOperators;
};
