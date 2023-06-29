// (c) Electronic Arts.  All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"

class FAssetTypeActions_TextureSet : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_TextureSet();
	virtual UClass* GetSupportedClass() const override;
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual uint32 GetCategories() override { return AssetCategoryBit; }
	virtual bool CanFilter() override;
private:
	uint32 AssetCategoryBit;

};
