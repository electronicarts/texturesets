// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"

class FAssetTypeActions_TextureSetDefinition : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_TextureSetDefinition();
	virtual UClass* GetSupportedClass() const override;
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual uint32 GetCategories() override { return AssetCategoryBit; }
	virtual bool CanFilter() override;

private:
	uint32 AssetCategoryBit;
};
