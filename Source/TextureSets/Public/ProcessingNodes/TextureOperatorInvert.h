// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "TextureOperator.h"

class FTextureOperatorInvert : public FTextureOperator
{
public:
	FTextureOperatorInvert(TSharedRef<ITextureProcessingNode> I) : FTextureOperator(I)
	{}

	virtual FName GetNodeTypeName() const  { return "Invert"; }

#if PROCESSING_METHOD == PROCESSING_METHOD_CHUNK
	virtual void ComputeChunk(const FTextureProcessingChunk& Chunk, float* TextureData) const override
	{
		SourceImage->ComputeChunk(Chunk, TextureData);

		for (int DataIndex = Chunk.DataStart; DataIndex <= Chunk.DataEnd; DataIndex += Chunk.DataPixelStride)
		{
			TextureData[DataIndex] = 1.0f - TextureData[DataIndex];
		}
	}
#else
	virtual float GetPixel(int X, int Y, int Z, int Channel) const override
	{
		return 1.0f - SourceImage->GetPixel(X, Y, Z, Channel);
	}
#endif
};
#endif
