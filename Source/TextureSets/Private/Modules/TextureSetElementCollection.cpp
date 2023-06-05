// (c) Electronic Arts. All Rights Reserved.

#include "Modules/TextureSetElementCollection.h"

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

void UTextureSetElementCollection::BuildSharedInfo(TextureSetDefinitionSharedInfo& Info)
{
	for (const FElementDefinition& Element: Elements)
	{
		TextureSetTextureDef TextureDef;
		TextureDef.Name = Element.ElementName;
		TextureDef.SRGB = Element.SRGB;
		TextureDef.ChannelCount = Element.ChannelCount;
		TextureDef.DefaultValue = Element.DefaultValue;
		Info.AddSourceTexture(TextureDef);
		Info.AddProcessedTexture(TextureDef);
	}
}

void UTextureSetElementCollection::BuildSamplingInfo(TextureSetDefinitionSamplingInfo& SamplingInfo, const UMaterialExpressionTextureSetSampleParameter* SampleExpression)
{
	const EMaterialValueType ValueTypeLookup[4] = { MCT_Float1, MCT_Float2, MCT_Float3, MCT_Float4 };

	for (const FElementDefinition& Element: Elements)
	{
		SamplingInfo.AddSampleOutput(Element.ElementName, ValueTypeLookup[Element.ChannelCount]);
	}
}

int32 UTextureSetElementCollection::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression)
{
	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);
	for (const FElementDefinition& Element: Elements)
	{
		Hash = HashCombine(Hash, GetTypeHash(Element));
	}
	
	return Hash;
}

#if WITH_EDITOR
void UTextureSetElementCollection::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	for (const FElementDefinition& Element: Elements)
	{
		// Simply connect texture sample to the matching output.
		TObjectPtr<UMaterialExpression> TextureExpression = Builder.GetProcessedTextureSample(Element.ElementName);
		TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = Builder.GetOutput(Element.ElementName);
		TextureExpression->ConnectExpression(OutputExpression->GetInput(0), 0);
	}
}
#endif
