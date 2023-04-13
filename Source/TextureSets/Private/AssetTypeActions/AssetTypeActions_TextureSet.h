//
// (c) Electronic Arts.  All Rights Reserved.
//

#pragma once

#include "AssetTypeActions_Base.h"
#include "TextureSet.h"

class FAssetTypeActions_TextureSet : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_TextureSet();
	virtual UClass* GetSupportedClass() const override;
	virtual FText   GetName() const override;
	virtual FColor  GetTypeColor() const override;
	virtual uint32  GetCategories() override { return AssetCategoryBit; }
	virtual bool    CanFilter() override;
	virtual bool    HasActions(const TArray<UObject*>& InObjects) const override;
	void ExecuteFindMaterials(TWeakObjectPtr<UTextureSet> Object);
	void ExecuteFixUsages(TWeakObjectPtr<UTextureSet> Object);
	virtual void    GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
private:
	uint32 AssetCategoryBit;
	
};
