// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetMaterialGraphBuilder.h"

#if WITH_EDITOR
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetModule.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialPropertyHelpers.h" // For access to FMaterialPropertyHelpers::TextureSetParamName only
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MaterialEditingLibrary.h"
#include "MaterialExpressionTextureStreamingDef.h"

FTextureSetMaterialGraphBuilder::FTextureSetMaterialGraphBuilder(UMaterialFunction* MaterialFunction, const UMaterialExpressionTextureSetSampleParameter* Node)
	: Node(Node)
	, Definition(Node->Definition)
	, MaterialFunction(MaterialFunction)
	, ModuleInfo(Definition->GetModuleInfo())
	, PackingInfo(Definition->GetPackingInfo())
{
	TObjectPtr<UMaterialExpressionFunctionInput> UVExpression = CreateExpression<UMaterialExpressionFunctionInput>();
	UVExpression->InputType = EFunctionInputType::FunctionInput_Vector2;
	UVExpression->InputName = TEXT("UV");

	const UTextureSet* DefaultTextureSet = Definition->GetDefaultTextureSet();

	// Create Texture Parameters for each packed texture
	for (int i = 0; i < PackingInfo.NumPackedTextures(); i++)
	{
		const FTextureSetPackedTextureDef& TextureDef = PackingInfo.GetPackedTextureDef(i);
		const FTextureSetPackedTextureInfo& TextureInfo = PackingInfo.GetPackedTextureInfo(i);
		const FName PackedTextureName = Node->GetTextureParameterName(i);

		TObjectPtr<UMaterialExpressionTextureObjectParameter> TextureObject = CreateExpression<UMaterialExpressionTextureObjectParameter>();
		UTexture* DefaultTexture = DefaultTextureSet->GetDerivedTexture(i);
		TextureObject->SamplerType = UMaterialExpressionTextureBase::GetSamplerTypeForTexture(DefaultTexture);
		TextureObject->SamplerSource = ESamplerSourceMode::SSM_Wrap_WorldGroupSettings; // So we don't allocate a sampler

		TextureObject->SetParameterName(PackedTextureName);
		FMaterialParameterMetadata Meta;
		Meta.Value.Type = EMaterialParameterType::Texture;
		Meta.Value.Texture = DefaultTexture;
		Meta.Group = Node->Group;
		Meta.SortPriority = 0;
		TextureObject->SetParameterValue(PackedTextureName, Meta, EMaterialExpressionSetParameterValueFlags::AssignGroupAndSortPriority);
		TextureObject->UpdateParameterGuid(true, true);
		PackedTextureObjects.Add(TextureObject);

		if (TextureDef.bDoRangeCompression)
		{
			MakeConstantParameter(TextureInfo.RangeCompressAddName, FVector4::Zero());
			MakeConstantParameter(TextureInfo.RangeCompressMulName, FVector4::One());
		}
	}

	// Sample each packed texture
	for (int t = 0; t < PackingInfo.NumPackedTextures(); t++)
	{
		const FTextureSetPackedTextureDef& TextureDef = PackingInfo.GetPackedTextureDef(t);
		TObjectPtr<UMaterialExpression> TextureSample = MakeTextureSamplerCustomNode(UVExpression, t);
		PackedTextureSamples.Add(TextureSample);

		const TArray<FName> SourceChannels = TextureDef.GetSources();
		for (int c = 0; c < SourceChannels.Num(); c++)
		{
			if (SourceChannels[c].IsNone())
				continue;

			UMaterialExpression* ChannelNode = TextureSample;
			int ChannelNodeOutput = c + 1;

			// Create a named node for this channel
			TObjectPtr<UMaterialExpressionNamedRerouteDeclaration> NamedNode = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
			NamedNode->Name = SourceChannels[c];
			ChannelNode->ConnectExpression(NamedNode->GetInput(0), ChannelNodeOutput);

			ProcessedTextureSamples.Add(SourceChannels[c], NamedNode);
		}
	}

	// Append vectors back into their equivalent processed map samples
	for (const auto& [Name, ProcessedTexture] : ModuleInfo.GetProcessedTextures())
	{
		const FName TextureName = Name;
		const int TextureChannels = ProcessedTexture.ChannelCount;

		// Only support 1-4 channels
		check(TextureChannels >= 1 && TextureChannels <= 4);

		FName TextureSampleName = FName(TextureName.ToString() + UTextureSetDefinition::ChannelSuffixes[0]);

		checkf(ProcessedTextureSamples.Contains(TextureSampleName), TEXT("Processed texture sample not found. This can happen if the texture was not packed into a valid channel."));

		UMaterialExpression* WorkingNode = ProcessedTextureSamples.FindChecked(TextureSampleName);

		if (TextureChannels == 1)
		{
			// For single channel textures, we can re-use the same node.
			// Just add it a second time to the map with a different key
			ProcessedTextureSamples.Add(TextureName, WorkingNode);
		}
		else
		{
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
	}

	// Call out to modules to do the work of connecting processed texture samples to outputs
	for (const UTextureSetModule* Module : Definition->GetModules())
	{
		Module->GenerateSamplingGraph(Node, *this);
	}
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetProcessedTextureSample(FName Name)
{
	return ProcessedTextureSamples.FindChecked(Name);
}

UMaterialExpressionFunctionOutput* FTextureSetMaterialGraphBuilder::CreateOutput(FName Name)
{
	check(!SampleOutputs.Contains(Name));

	UMaterialExpressionFunctionOutput* NewOutput = CreateExpression<UMaterialExpressionFunctionOutput>();
	NewOutput->OutputName = Name;
	SampleOutputs.Add(Name, NewOutput);
	return NewOutput;
}

UMaterialExpressionFunctionOutput* FTextureSetMaterialGraphBuilder::GetOutput(FName Name)
{
	return SampleOutputs.FindChecked(Name);
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::MakeConstantParameter(FName Name, FVector4 Default)
{
	FName ParameterName = Node->GetConstantParameterName(Name);

	UMaterialExpressionVectorParameter* NewParam = CreateExpression<UMaterialExpressionVectorParameter>();
	NewParam->ParameterName = ParameterName;
	NewParam->DefaultValue = FLinearColor(Default);
	NewParam->Group = Node->Group;
	
	// Main pin on the parameter is actually a float3, need to append the alpha to make it a float4.
	UMaterialExpression* AppendNode = CreateExpression<UMaterialExpressionAppendVector>();
	NewParam->ConnectExpression(AppendNode->GetInput(0), 0);
	NewParam->ConnectExpression(AppendNode->GetInput(1), 4);
	
	ConstantParameters.Add(Name, AppendNode);
	return AppendNode;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::MakeTextureSamplerCustomNode(UMaterialExpression* Texcoord, int Index)
{
	const FTextureSetPackedTextureDef& TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo& TextureInfo = PackingInfo.GetPackedTextureInfo(Index);
	UMaterialExpressionTextureObjectParameter* TextureObject = PackedTextureObjects[Index];

	UMaterialExpressionTextureStreamingDef* TextureStreamingDef = CreateExpression<UMaterialExpressionTextureStreamingDef>();
	TextureStreamingDef->SamplerType = TextureObject->SamplerType;
	TextureStreamingDef->SamplerSource = ESamplerSourceMode::SSM_Wrap_WorldGroupSettings;// So we don't allocate a sampler
	Texcoord->ConnectExpression(TextureStreamingDef->GetInput(0), 0);
	TextureObject->ConnectExpression(TextureStreamingDef->GetInput(1), 0);

	UMaterialExpressionCustom* CustomExp = CreateExpression<UMaterialExpressionCustom>();

	CustomExp->Inputs.Empty(); // required: class initializes with one input by default
	CustomExp->Inputs.Add({"Texcoord"});
	CustomExp->Inputs.Add({"Tex"});

	static const FString ChannelSuffixLower[4] {"r", "g", "b", "a"};
	static const FString ChannelSuffixUpper[4] {"R", "G", "B", "A"};

	// Do the sample and set in the correct size of float for how many channels we have
	CustomExp->Code = "float" + FString::FromInt(TextureInfo.ChannelCount) + " Sample = Tex.Sample(TexSampler, Texcoord).";
	
	for (int c = 0; c < TextureInfo.ChannelCount; c++)
		CustomExp->Code += ChannelSuffixLower[c];

	CustomExp->Code += ";\n";

	int RangeCompressMulInput = 0;
	int RangeCompressAddInput = 0;

	// Process each channel
	for (int c = 0; c < TextureInfo.ChannelCount; c++)
	{
		TArray<FStringFormatArg> FormatArgs = {ChannelSuffixLower[c], ChannelSuffixUpper[c]};

		// Decode channel based on which encoding it uses
		if (TextureInfo.ChannelInfo[c].ChannelEncoding == ETextureSetTextureChannelEncoding::Linear_RangeCompressed)
		{
			if (RangeCompressMulInput == 0)
			{
				RangeCompressMulInput = CustomExp->Inputs.Num();
				CustomExp->Inputs.Add({"RangeCompressMul"});
				RangeCompressAddInput = CustomExp->Inputs.Num();
				CustomExp->Inputs.Add({"RangeCompressAdd"});
			}

			CustomExp->Code += FString::Format(TEXT("Sample.{0} = Sample.{0} * RangeCompressMul.{0} + RangeCompressAdd.{0};\n"), FormatArgs);
		}
		else if (TextureInfo.ChannelInfo[c].ChannelEncoding == ETextureSetTextureChannelEncoding::SRGB && (!TextureInfo.HardwareSRGB || c >= 3))
		{
			// Need to do sRGB decompression in the shader
			CustomExp->Code += FString::Format(TEXT("Sample.{0} = pow(Sample.{0}, 2.2f);\n"), FormatArgs);
		}

		// Add an output pin for this channel
		CustomExp->AdditionalOutputs.Add(FCustomOutput({FName(ChannelSuffixUpper[c]), ECustomMaterialOutputType::CMOT_Float1}));
		CustomExp->Code += FString::Format(TEXT("{1} = Sample.{0};\n"), FormatArgs);
	}

	// Return the correct output type
	static const ECustomMaterialOutputType OutputTypes[4] {CMOT_Float1, CMOT_Float2, CMOT_Float3, CMOT_Float4};
	CustomExp->OutputType = OutputTypes[TextureInfo.ChannelCount - 1];
	CustomExp->Code += "return Sample;";

	CustomExp->RebuildOutputs();

	TextureStreamingDef->ConnectExpression(CustomExp->GetInput(0), 0);
	TextureObject->ConnectExpression(CustomExp->GetInput(1), 0);

	if (RangeCompressMulInput != 0)
	{
		ConstantParameters.FindChecked(TextureInfo.RangeCompressMulName)->ConnectExpression(CustomExp->GetInput(RangeCompressMulInput), 0);
		ConstantParameters.FindChecked(TextureInfo.RangeCompressAddName)->ConnectExpression(CustomExp->GetInput(RangeCompressAddInput), 0);
	}

	return CustomExp;
}
#endif // WITH_EDITOR
