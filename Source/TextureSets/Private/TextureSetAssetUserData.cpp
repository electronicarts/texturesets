// (c) Electronic Arts. All Rights Reserved.


#include "TextureSetAssetUserData.h"

void UTextureSetAssetUserData::AddOverride(FSetOverride& Override)
{
	TexturesSetOverrides.Add(Override);
}

void UTextureSetAssetUserData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	for (auto& Override : TexturesSetOverrides)
	{
		Override.TextureSet = Override.TextureSet ? Override.TextureSet : Override.DefaultTextureSet;
	}
}
