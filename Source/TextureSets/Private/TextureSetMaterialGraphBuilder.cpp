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
#include "Materials/MaterialExpressionCustom.h"
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

		TObjectPtr<UMaterialExpression> TextureSample = MakeTextureSamplerCustomNode(UVExpression, TextureObject);
		PackedTextureSamples.Add(TextureSample);

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

UMaterialExpression* FTextureSetMaterialGraphBuilder::MakeTextureSamplerCustomNode(UMaterialExpression* Texcoord, UMaterialExpression* TexObject)
{
	UMaterialExpressionCustom* CustomExp = CreateExpression<UMaterialExpressionCustom>();

	CustomExp->Inputs.Empty(); // required: class initializes with one input by default
	CustomExp->Inputs.Add({"Texcoord"});
	CustomExp->Inputs.Add({"Tex"});
	
	CustomExp->OutputType = CMOT_Float4;
	CustomExp->AdditionalOutputs.Add(FCustomOutput({"R", ECustomMaterialOutputType::CMOT_Float1}));
	CustomExp->AdditionalOutputs.Add(FCustomOutput({"G", ECustomMaterialOutputType::CMOT_Float1}));
	CustomExp->AdditionalOutputs.Add(FCustomOutput({"B", ECustomMaterialOutputType::CMOT_Float1}));
	CustomExp->AdditionalOutputs.Add(FCustomOutput({"A", ECustomMaterialOutputType::CMOT_Float1}));

	CustomExp->Code =
		TEXT("\
		float4 Sample = Tex.Sample(TexSampler, Texcoord).rgba;\n\
		R = Sample.r;\n\
		G = Sample.g;\n\
		B = Sample.b;\n\
		A = Sample.a;\n\
		return Sample;");

	CustomExp->RebuildOutputs();

	Texcoord->ConnectExpression(CustomExp->GetInput(0), 0);
	TexObject->ConnectExpression(CustomExp->GetInput(1), 0);

	return CustomExp;
}
