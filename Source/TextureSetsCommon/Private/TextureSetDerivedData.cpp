// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetDerivedData.h"

void UTextureSetDerivedData::PostLoad()
{
	for (const FDerivedTexture& DerivedTexture : Textures)
	{
		Super::PostLoad();

		if (IsValid(DerivedTexture.Texture))
			DerivedTexture.Texture->ConditionalPostLoad();
	}
}