// (c) Electronic Arts. All Rights Reserved.


#include "TextureSetAssetUserData.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"

void UTextureSetAssetUserData::AddOverride(FSetOverride& Override)
{
	TexturesSetOverrides.Add(Override);
}

void UTextureSetAssetUserData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	for (auto& Override : TexturesSetOverrides)
	{
		//Override.IsOverridden = (Override.TextureSet != nullptr) && (Override.TextureSet != Override.DefaultTextureSet);
		Override.TextureSet = Override.TextureSet ? Override.TextureSet : Override.DefaultTextureSet;
	}
}
