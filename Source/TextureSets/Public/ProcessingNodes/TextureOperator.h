// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "IProcessingNode.h"

class FTextureOperator : public ITextureProcessingNode
{
public:
	FTextureOperator(const TSharedRef<ITextureProcessingNode> I)
		: SourceImage(I)
	{}

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override { SourceImage->LoadResources(Context); }
	virtual void Initialize(const FTextureSetProcessingContext& Context) override { SourceImage->Initialize(Context); }

	virtual const uint32 ComputeGraphHash() const override { return HashCombine(SourceImage->ComputeGraphHash(), GetTypeHash(GetNodeTypeName().ToString())); }
	virtual const uint32 ComputeDataHash(const FTextureSetProcessingContext& Context) const override { return SourceImage->ComputeDataHash(Context); };

	virtual int GetWidth() const override { return SourceImage->GetWidth(); }
	virtual int GetHeight() const override { return SourceImage->GetHeight(); }
	virtual int GetSlices() const override { return SourceImage->GetSlices(); }
	virtual const FTextureSetProcessedTextureDef& GetTextureDef() override { return SourceImage->GetTextureDef(); }

	const TSharedRef<ITextureProcessingNode> SourceImage;
};
#endif
