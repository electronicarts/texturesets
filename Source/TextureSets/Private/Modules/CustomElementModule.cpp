// (c) Electronic Arts. All Rights Reserved.

#include "Modules/CustomElementModule.h"

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

TArray<TextureSetTextureDef> UCustomElementModule::GetSourceTextures() const
{
	return TArray<TextureSetTextureDef> { TextureSetTextureDef{ ElementName, SRGB, ChannelCount, DefaultValue } };
}

void UCustomElementModule::CollectSampleOutputs(TMap<FName, EMaterialValueType>& Results, const UMaterialExpressionTextureSetSampleParameter* SampleParams) const
{
	const EMaterialValueType ValueTypeLookup[4] = { MCT_Float1, MCT_Float2, MCT_Float3, MCT_Float4 };

	Results.Add(ElementName, ValueTypeLookup[ChannelCount]);
}

void UCustomElementModule::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	// Simply connect texture sample to the matching output.
	TObjectPtr<UMaterialExpression> TextureExpression = Builder.GetProcessedTextureSample(ElementName);
	TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = Builder.GetOutput(ElementName);
	TextureExpression->ConnectExpression(OutputExpression->GetInput(0), 0);
}