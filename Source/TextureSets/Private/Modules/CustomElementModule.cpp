// (c) Electronic Arts. All Rights Reserved.

#include "Modules/CustomElementModule.h"

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

void UCustomElementModule::BuildSharedInfo(TextureSetDefinitionSharedInfo& Info)
{
	TextureSetTextureDef TextureDef = TextureSetTextureDef{ ElementName, SRGB, ChannelCount, DefaultValue };
	Info.AddSourceTexture(TextureDef);
	Info.AddProcessedTexture(TextureDef);
}

void UCustomElementModule::BuildSamplingInfo(TextureSetDefinitionSamplingInfo& SamplingInfo, const UMaterialExpressionTextureSetSampleParameter* SampleExpression)
{
	const EMaterialValueType ValueTypeLookup[4] = { MCT_Float1, MCT_Float2, MCT_Float3, MCT_Float4 };
	SamplingInfo.AddSampleOutput(ElementName, ValueTypeLookup[ChannelCount]);
}

void UCustomElementModule::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	// Simply connect texture sample to the matching output.
	TObjectPtr<UMaterialExpression> TextureExpression = Builder.GetProcessedTextureSample(ElementName);
	TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = Builder.GetOutput(ElementName);
	TextureExpression->ConnectExpression(OutputExpression->GetInput(0), 0);
}