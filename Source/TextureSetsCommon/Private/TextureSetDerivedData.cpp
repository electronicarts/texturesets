// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetDerivedData.h"

#include "Engine/Texture.h"

void UTextureSetDerivedData::PostLoad()
{
	for (const FDerivedTexture& DerivedTexture : Textures)
	{
		Super::PostLoad();

		if (IsValid(DerivedTexture.Texture))
			DerivedTexture.Texture->ConditionalPostLoad();
	}
}