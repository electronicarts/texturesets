// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetMaterialGraphBuilder.h"

#include "TextureSetDefinition.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialPropertyHelpers.h" // For access to FMaterialPropertyHelpers::TextureSetParamName only
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "MaterialEditingLibrary.h"

FTextureSetMaterialGraphBuilder::FTextureSetMaterialGraphBuilder(TObjectPtr<UTextureSetDefinition> Def, UMaterialExpressionTextureSetSampleParameter* Node)
	: Definition(Def)
{
	check(Definition);
	TextureSetDefinitionSharedInfo SharedInfo = Definition->GetSharedInfo();
	TextureSetDefinitionSamplingInfo SamplingInfo = Definition->GetSamplingInfo(Node);

	FName FunctionName = MakeUniqueObjectName(Node, UMaterialFunction::StaticClass(), Node->ParameterName, EUniqueObjectNameOptions::GloballyUnique);
	MaterialFunction = NewObject<UMaterialFunction>(Node, FunctionName);

	TObjectPtr<UMaterialExpressionFunctionInput> UVExpression = CreateExpression<UMaterialExpressionFunctionInput>();
	UVExpression->InputType = EFunctionInputType::FunctionInput_Vector2;
	UVExpression->InputName = TEXT("UV");

	// Create Texture Parameters for each packed texture
	for (int i = 0; i < Definition->PackedTextures.Num(); i++)
	{
		const FName PackedTextureName = FName(Node->ParameterName.ToString() + "_PACKED_" + FString::FromInt(i));

		TObjectPtr<UMaterialExpressionTextureObjectParameter> TextureObject = CreateExpression<UMaterialExpressionTextureObjectParameter>();

		TextureObject->SetParameterName(PackedTextureName);
		FMaterialParameterMetadata meta;
		meta.Value.Type = EMaterialParameterType::Texture;
		meta.Value.Texture = Definition->GetDefaultPackedTexture(i);
		meta.Group = FMaterialPropertyHelpers::TextureSetParamName;
		meta.SortPriority = 0;
		TextureObject->SetParameterValue(PackedTextureName, meta, EMaterialExpressionSetParameterValueFlags::AssignGroupAndSortPriority);
		TextureObject->UpdateParameterGuid(true, true);
		PackedTextureObjects.Add(TextureObject);
	}

	// Sample each packed texture
	for (int t = 0; t < Definition->PackedTextures.Num(); t++)
	{
		const FTextureSetPackedTextureDef& PackedTexture = Definition->PackedTextures[t];
		UMaterialExpressionTextureObjectParameter* TextureObject = PackedTextureObjects[t];

		TObjectPtr<UMaterialExpressionTextureSample> TextureSample = CreateExpression<UMaterialExpressionTextureSample>();
		PackedTextureSamples.Add(TextureSample);
		UVExpression->ConnectExpression(TextureSample->GetInput(0), 0);
		TextureObject->ConnectExpression(TextureSample->GetInput(1), 0);

		const TArray<FName> SourceChannels = PackedTexture.GetSources();
		for (int c = 0; c < SourceChannels.Num(); c++)
		{
			UMaterialExpression* ChannelNode = TextureSample;
			int ChannelNodeOutput = c + 1;

			// TODO Range Compression
			// TODO sRGB

			// Create a named node for this channel
			TObjectPtr<UMaterialExpressionNamedRerouteDeclaration> NamedNode = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
			NamedNode->Name = SourceChannels[c];
			ChannelNode->ConnectExpression(NamedNode->GetInput(0), ChannelNodeOutput);

			ProcessedTextureSamples.Add(SourceChannels[c], NamedNode);
		}
	}

	// Append vectors back into their equivalent processed map samples
	for (const auto& ProcessedTexture : SharedInfo.GetProcessedTextures())
	{
		const FName TextureName = ProcessedTexture.Name;
		const int TextureChannels = ProcessedTexture.ChannelCount;

		// Only support 1-4 channels
		check(TextureChannels >= 1 && TextureChannels <= 4);

		UMaterialExpression* WorkingNode = ProcessedTextureSamples.FindChecked(FName(TextureName.ToString() + UTextureSetDefinition::ChannelSuffixes[0]));

		for (int i = 1; i < TextureChannels; i++)
		{
			UMaterialExpression* AppendNode = CreateExpression<UMaterialExpressionAppendVector>();
			UMaterialExpression* NextChannel = ProcessedTextureSamples.FindChecked(FName(TextureName.ToString() + UTextureSetDefinition::ChannelSuffixes[i]));
			WorkingNode->ConnectExpression(AppendNode->GetInput(0), 0);
			NextChannel->ConnectExpression(AppendNode->GetInput(1), 0);
			WorkingNode = AppendNode;
		}

		TObjectPtr<UMaterialExpressionNamedRerouteDeclaration> NamedNode = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
		NamedNode->Name = TextureName;
		WorkingNode->ConnectExpression(NamedNode->GetInput(0), 0);
		ProcessedTextureSamples.Add(TextureName, NamedNode);
	}

	// Create function outputs
	for (const auto& Result : SamplingInfo.GetSampleOutputs())
	{
		TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = CreateExpression<UMaterialExpressionFunctionOutput>();
		OutputExpression->OutputName = Result.Key;
		SampleOutputs.Add(Result.Key, OutputExpression);
	}
}

void FTextureSetMaterialGraphBuilder::Finalize()
{
	UMaterialEditingLibrary::LayoutMaterialFunctionExpressions(MaterialFunction);
}
