// (c) Electronic Arts. All Rights Reserved.

#include "Modules/TextureSetElementCollection.h"

TArray<TextureSetTextureDef> UTextureSetElementCollection::GetSourceTextures() const
{
	TArray<TextureSetTextureDef> SourceTextures;

	for (const FElementDefinition& Element: Elements)
	{
		TextureSetTextureDef Def;
		Def.Name = Element.ElementName;
		Def.SRGB = Element.SRGB;
		Def.ChannelCount = Element.ChannelCount;
		Def.DefaultValue = Element.DefaultValue;
		SourceTextures.Add(Def);
	}

	return SourceTextures;
}

void UTextureSetElementCollection::CollectSampleOutputs(TMap<FName, EMaterialValueType>& Results, const UMaterialExpressionTextureSetSampleParameter* SampleParams) const
{
	const EMaterialValueType ValueTypeLookup[4] = { MCT_Float1, MCT_Float2, MCT_Float3, MCT_Float4 };

	for (const FElementDefinition& Element: Elements)
	{
		Results.Add(Element.ElementName, ValueTypeLookup[Element.ChannelCount]);
	}
}