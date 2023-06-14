// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetMaterialGraphBuilder.h"

#if WITH_EDITOR
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
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MaterialEditingLibrary.h"
#include "MaterialExpressionTextureStreamingDef.h"

FTextureSetMaterialGraphBuilder::FTextureSetMaterialGraphBuilder(UMaterialExpressionTextureSetSampleParameter* Node)
	: Node(Node)
	, Definition(Node->Definition)
	, SharedInfo(Definition->GetSharedInfo())
	, SamplingInfo(Definition->GetSamplingInfo(Node))
	, PackingInfo(Definition->GetPackingInfo())
{

	FName FunctionName = MakeUniqueObjectName(Node, UMaterialFunction::StaticClass(), Node->ParameterName, EUniqueObjectNameOptions::GloballyUnique);
	EObjectFlags Flags = RF_Transient; // Ensure it's never saved
	MaterialFunction = NewObject<UMaterialFunction>(Node, FunctionName, Flags);

	TObjectPtr<UMaterialExpressionFunctionInput> UVExpression = CreateExpression<UMaterialExpressionFunctionInput>();
	UVExpression->InputType = EFunctionInputType::FunctionInput_Vector2;
	UVExpression->InputName = TEXT("UV");

	// Create Texture Parameters for each packed texture
	for (int i = 0; i < PackingInfo.NumPackedTextures(); i++)
	{
		const FTextureSetPackedTextureDef& TextureDef = PackingInfo.GetPackedTextureDef(i);
		const TextureSetPackingInfo::TextureSetPackedTextureInfo& TextureInfo = PackingInfo.GetPackedTextureInfo(i);
		const FName PackedTextureName = Node->GetTextureParameterName(i);

		TObjectPtr<UMaterialExpressionTextureObjectParameter> TextureObject = CreateExpression<UMaterialExpressionTextureObjectParameter>();
		UTexture* DefaultTexture = Definition->GetDefaultPackedTexture(i);
		TextureObject->SamplerType = UMaterialExpressionTextureBase::GetSamplerTypeForTexture(DefaultTexture);
		TextureObject->SamplerSource = ESamplerSourceMode::SSM_Wrap_WorldGroupSettings; // So we don't allocate a sampler

		TextureObject->SetParameterName(PackedTextureName);
		FMaterialParameterMetadata meta;
		meta.Value.Type = EMaterialParameterType::Texture;
		meta.Value.Texture = DefaultTexture;
		meta.Group = FMaterialPropertyHelpers::TextureSetParamName;
		meta.SortPriority = 0;
		TextureObject->SetParameterValue(PackedTextureName, meta, EMaterialExpressionSetParameterValueFlags::AssignGroupAndSortPriority);
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
			UMaterialExpression* ChannelNode = TextureSample;
			int ChannelNodeOutput = c + 1;

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

UMaterialExpression* FTextureSetMaterialGraphBuilder::MakeConstantParameter(FName Name, FVector4 Default)
{
	FMaterialParameterMetadata meta;
	meta.SortPriority = 0;

	FName ParameterName = Node->GetConstantParameterName(Name);

	UMaterialExpressionVectorParameter* NewParam = CreateExpression<UMaterialExpressionVectorParameter>();
	NewParam->ParameterName = ParameterName;
	NewParam->DefaultValue = FLinearColor(Default);
	NewParam->Group = FMaterialPropertyHelpers::TextureSetParamName;
	
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
	const TextureSetPackingInfo::TextureSetPackedTextureInfo& TextureInfo = PackingInfo.GetPackedTextureInfo(Index);
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

	CustomExp->Code = "float4 Sample = Tex.Sample(TexSampler, Texcoord).rgba;\n";

	// TODO: sRGB when hardware sRGB has not been used.

	int RangeCompressMulInput = 0;
	int RangeCompressAddInput = 0;

	if (TextureDef.bDoRangeCompression)
	{
		RangeCompressMulInput = CustomExp->Inputs.Num();
		CustomExp->Inputs.Add({"RangeCompressMul"});
		RangeCompressAddInput = CustomExp->Inputs.Num();
		CustomExp->Inputs.Add({"RangeCompressAdd"});

		CustomExp->Code += "Sample = Sample * RangeCompressMul + RangeCompressAdd;\n";
	}
	
	CustomExp->AdditionalOutputs.Add(FCustomOutput({"R", ECustomMaterialOutputType::CMOT_Float1}));
	CustomExp->Code += "R = Sample.r;\n";
	CustomExp->AdditionalOutputs.Add(FCustomOutput({"G", ECustomMaterialOutputType::CMOT_Float1}));
	CustomExp->Code += "G = Sample.g;\n";
	CustomExp->AdditionalOutputs.Add(FCustomOutput({"B", ECustomMaterialOutputType::CMOT_Float1}));
	CustomExp->Code += "B = Sample.b;\n";
	CustomExp->AdditionalOutputs.Add(FCustomOutput({"A", ECustomMaterialOutputType::CMOT_Float1}));
	CustomExp->Code += "A = Sample.a;\n";
	CustomExp->OutputType = CMOT_Float4;
	CustomExp->Code += "return Sample;";

	CustomExp->RebuildOutputs();

	TextureStreamingDef->ConnectExpression(CustomExp->GetInput(0), 0);
	TextureObject->ConnectExpression(CustomExp->GetInput(1), 0);

	if (TextureDef.bDoRangeCompression)
	{
		ConstantParameters.FindChecked(TextureInfo.RangeCompressMulName)->ConnectExpression(CustomExp->GetInput(RangeCompressMulInput), 0);
		ConstantParameters.FindChecked(TextureInfo.RangeCompressAddName)->ConnectExpression(CustomExp->GetInput(RangeCompressAddInput), 0);
	}

	return CustomExp;
}
#endif // WITH_EDITOR
