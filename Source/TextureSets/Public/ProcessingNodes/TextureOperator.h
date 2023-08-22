// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IProcessingNode.h"

class FTextureOperator : public IProcessingNode
{
public:
	FTextureOperator(const TSharedRef<IProcessingNode> I)
		: SourceImage(I)
	{}

	virtual void Initialize(const FTextureSetProcessingContext& Context) { SourceImage->Initialize(Context); }

	virtual int GetWidth() const override { return SourceImage->GetWidth(); }
	virtual int GetHeight() const override { return SourceImage->GetHeight(); }
	virtual const FTextureSetProcessedTextureDef& GetTextureDef() override { return SourceImage->GetTextureDef(); }

	const TSharedRef<IProcessingNode> SourceImage;
};
