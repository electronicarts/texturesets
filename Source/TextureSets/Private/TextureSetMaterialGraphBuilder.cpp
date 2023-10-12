// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetMaterialGraphBuilder.h"

#if WITH_EDITOR
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialExpressionTextureStreamingDef.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDDX.h"
#include "Materials/MaterialExpressionDDY.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureProperty.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVertexTangentWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetModule.h"
#include "TextureSetsHelpers.h"

FTextureSetMaterialGraphBuilder::FTextureSetMaterialGraphBuilder(UMaterialFunction* MaterialFunction, const UMaterialExpressionTextureSetSampleParameter* Node)
	: Node(Node)
	, Definition(Node->Definition)
	, MaterialFunction(MaterialFunction)
	, ModuleInfo(Definition->GetModuleInfo())
	, PackingInfo(Definition->GetPackingInfo())
{
	TexcoordRaw = CreateInput("UV", EFunctionInputType::FunctionInput_Vector2, EInputSort::UV);
	TexcoordReroute = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
	TexcoordReroute->Name = "Texcoord";
	TexcoordStreamingReroute = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
	TexcoordStreamingReroute->Name = "Streaming Texcoord";
	TexcoordDerivativeX = CreateExpression<UMaterialExpressionDDX>();
	TexcoordDerivativeY = CreateExpression<UMaterialExpressionDDY>();
	
	OverrideTexcoord(TexcoordRaw, true, true);

	const UTextureSet* DefaultTextureSet = Definition->GetDefaultTextureSet();

	// Create Texture Parameters for each packed texture
	for (int i = 0; i < PackingInfo.NumPackedTextures(); i++)
	{
		const FTextureSetPackedTextureDef& TextureDef = PackingInfo.GetPackedTextureDef(i);
		const FTextureSetPackedTextureInfo& TextureInfo = PackingInfo.GetPackedTextureInfo(i);
		const FName PackedTextureName = Node->GetTextureParameterName(i);

		TObjectPtr<UMaterialExpressionTextureObjectParameter> TextureObject = CreateExpression<UMaterialExpressionTextureObjectParameter>();
		UTexture* DefaultTexture = DefaultTextureSet->GetDerivedData().Textures[i];
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
			MakeConstantParameter(TextureInfo.RangeCompressAddName, FVector4f::Zero());
			MakeConstantParameter(TextureInfo.RangeCompressMulName, FVector4f::One());
		}
	}

	// Sample each packed texture
	for (int t = 0; t < PackingInfo.NumPackedTextures(); t++)
	{
		const FTextureSetPackedTextureDef& TextureDef = PackingInfo.GetPackedTextureDef(t);
		TObjectPtr<UMaterialExpression> TextureSample = MakeTextureSamplerCustomNode(t);
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

			// Record packing sources for lookup later.
			PackingSource.Add(SourceChannels[c], TTuple<int, int>(t, c));
		}
	}

	// Created on demand
	PackedTextureSizes.Init(nullptr, PackingInfo.NumPackedTextures());

	// Append vectors back into their equivalent processed map samples
	for (const auto& [Name, ProcessedTexture] : ModuleInfo.GetProcessedTextures())
	{
		const FName TextureName = Name;
		const int TextureChannels = ProcessedTexture.ChannelCount;

		// Only support 1-4 channels
		check(TextureChannels >= 1 && TextureChannels <= 4);

		FName TextureSampleName = FName(TextureName.ToString() + TextureSetsHelpers::ChannelSuffixes[0]);

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
				UMaterialExpression* NextChannel = ProcessedTextureSamples.FindChecked(FName(TextureName.ToString() + TextureSetsHelpers::ChannelSuffixes[i]));
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
	for (const UTextureSetModule* Module : Definition->GetModuleInfo().GetModules())
	{
		Module->GenerateSamplingGraph(Node, *this);
	}

	// Hook up proper inputs to any reroute nodes that were used.
	// Note: Order here is important, as we have some dependencies

	const bool TangentUsed = IsValid(TangentReroute) && TangentReroute->HasConnectedOutputs();
	const bool BitangentUsed = IsValid(BitangentReroute) && BitangentReroute->HasConnectedOutputs();

	if (TangentUsed || BitangentUsed)
	{
		switch (Node->TangentSource)
		{
		case ETangentSource::Explicit:
		{
			CreateInput("Tangent", EFunctionInputType::FunctionInput_Vector3, EInputSort::Tangent)->ConnectExpression(GetTangent()->GetInput(0), 0);
			CreateInput("Bitangent", EFunctionInputType::FunctionInput_Vector3, EInputSort::Tangent)->ConnectExpression(GetBitangent()->GetInput(0), 0);
			break;
		}
		case ETangentSource::Synthesized:
		{
			// Important this is done before doing the base normal reroute below, because MakeSynthesizeTangentCustomNode calls GetBaseNormal()
			UMaterialExpression* SynthesizeTangentNode = MakeSynthesizeTangentCustomNode();
			SynthesizeTangentNode->ConnectExpression(GetTangent()->GetInput(0), 1);
			SynthesizeTangentNode->ConnectExpression(GetBitangent()->GetInput(0), 2);
			break;
		}
		case ETangentSource::Vertex:
		{
			CreateExpression<UMaterialExpressionVertexTangentWS>()->ConnectExpression(GetTangent()->GetInput(0), 0);
			// Don't have a node explicitly for the vertex bitangent, so we create it by transforming a vector from tangent to world space
			UMaterialExpressionConstant3Vector* BitangentConstant = CreateExpression<UMaterialExpressionConstant3Vector>();
			BitangentConstant->Constant = FLinearColor(0, 1, 0);
			UMaterialExpressionTransform* TransformBitangent = CreateExpression<UMaterialExpressionTransform>();
			TransformBitangent->TransformSourceType = EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_Tangent;
			TransformBitangent->TransformType = EMaterialVectorCoordTransform::TRANSFORM_World;
			BitangentConstant->ConnectExpression(TransformBitangent->GetInput(0), 0);
			TransformBitangent->ConnectExpression(GetBitangent()->GetInput(0), 0);
			break;
		}
		default:
			unimplemented();
			break;
		}
	}

	if (IsValid(BaseNormalReroute) && BaseNormalReroute->HasConnectedOutputs())
	{
		switch (Node->BaseNormalSource)
		{
		case EBaseNormalSource::Explicit:
			CreateInput("Base Normal", EFunctionInputType::FunctionInput_Vector3, EInputSort::BaseNormal)->ConnectExpression(BaseNormalReroute->GetInput(0), 0);
			break;
		case EBaseNormalSource::Vertex:
			CreateExpression<UMaterialExpressionVertexNormalWS>()->ConnectExpression(BaseNormalReroute->GetInput(0), 0);
			break;
		default:
			unimplemented();
			break;
		}
	}

	if (IsValid(CameraVectorReroute) && CameraVectorReroute->HasConnectedOutputs())
	{
		switch (Node->CameraVectorSource)
		{
		case ECameraVectorSource::Explicit:
			CreateInput("Camera Vector", EFunctionInputType::FunctionInput_Vector3, EInputSort::CameraVector)->ConnectExpression(CameraVectorReroute->GetInput(0), 0);
			break;
		case ECameraVectorSource::World:
			CreateExpression<UMaterialExpressionCameraVectorWS>()->ConnectExpression(CameraVectorReroute->GetInput(0), 0);
			break;
		default:
			unimplemented();
			break;
		}
	}

	if (IsValid(PositionReroute) && PositionReroute->HasConnectedOutputs())
	{
		switch (Node->PositionSource)
		{
		case EPositionSource::Explicit:
			CreateInput("Position", EFunctionInputType::FunctionInput_Vector3, EInputSort::Position)->ConnectExpression(PositionReroute->GetInput(0), 0);
			break;
		case EPositionSource::World:
			CreateExpression<UMaterialExpressionWorldPosition>()->ConnectExpression(PositionReroute->GetInput(0), 0);
			break;
		default:
			unimplemented();
			break;
		}
	}
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetTexcoord()
{
	return TexcoordReroute;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetRawTexcoord() const
{
	return TexcoordRaw;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetTexcoordDerivativeX() const
{
	return TexcoordDerivativeX;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetTexcoordDerivativeY() const
{
	return TexcoordDerivativeY;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetTexcoordStreaming() const
{
	return TexcoordStreamingReroute;
}

void FTextureSetMaterialGraphBuilder::OverrideTexcoord(UMaterialExpression* NewTexcoord, bool OverrideDerivatives, bool OverrideStreaming)
{
	NewTexcoord->ConnectExpression(TexcoordReroute->GetInput(0), 0);

	if (OverrideDerivatives)
	{
		NewTexcoord->ConnectExpression(TexcoordDerivativeX->GetInput(0), 0);
		NewTexcoord->ConnectExpression(TexcoordDerivativeY->GetInput(0), 0);
	}

	if (OverrideStreaming)
	{
		NewTexcoord->ConnectExpression(TexcoordStreamingReroute->GetInput(0), 0);
	}
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetBaseNormal()
{
	if (!IsValid(BaseNormalReroute))
	{
		BaseNormalReroute = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
		BaseNormalReroute->Name = "Base Normal";
	}

	return BaseNormalReroute;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetTangent()
{
	if (!IsValid(TangentReroute))
	{
		TangentReroute = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
		TangentReroute->Name = "Tangent";
	}

	return TangentReroute;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetBitangent()
{
	if (!IsValid(BitangentReroute))
	{
		BitangentReroute = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
		BitangentReroute->Name = "Bitangent";
	}

	return BitangentReroute;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetCameraVector()
{
	if (!IsValid(CameraVectorReroute))
	{
		CameraVectorReroute = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
		CameraVectorReroute->Name = "CameraVector";
	}

	return CameraVectorReroute;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetPosition()
{
	if (!IsValid(PositionReroute))
	{
		PositionReroute = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
		PositionReroute->Name = "Position";
	}

	return PositionReroute;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetProcessedTextureSample(FName Name)
{
	return ProcessedTextureSamples.FindChecked(Name);
}

UMaterialExpressionTextureObjectParameter* FTextureSetMaterialGraphBuilder::GetPackedTextureObject(int Index)
{
	return PackedTextureObjects[Index];
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::GetPackedTextureSize(int Index)
{
	if (PackedTextureSizes[Index] == nullptr)
	{
		UMaterialExpressionTextureProperty* TextureProp = CreateExpression<UMaterialExpressionTextureProperty>();
		GetPackedTextureObject(Index)->ConnectExpression(TextureProp->GetInput(0), 0);
		TextureProp->Property = EMaterialExposedTextureProperty::TMTM_TextureSize;
		PackedTextureSizes[Index] = TextureProp;
	}

	return PackedTextureSizes[Index];
}

TTuple<int, int> FTextureSetMaterialGraphBuilder::GetPackingSource(FName ProcessedTextureChannel)
{
	return PackingSource.FindChecked(ProcessedTextureChannel);
}

UMaterialExpressionFunctionInput* FTextureSetMaterialGraphBuilder::CreateInput(FName Name, EFunctionInputType Type, EInputSort Sort)
{
	check(!SampleInputs.Contains(Name));

	UMaterialExpressionFunctionInput* NewInput = CreateExpression<UMaterialExpressionFunctionInput>();
	NewInput->InputName = Name;
	NewInput->InputType = Type;
	NewInput->SortPriority = (int)Sort;
	SampleInputs.Add(Name, NewInput);
	return NewInput;
}

UMaterialExpressionFunctionInput* FTextureSetMaterialGraphBuilder::GetInput(FName Name) const
{
	if (!SampleInputs.Contains(Name))
		return nullptr;

	return SampleInputs.FindChecked(Name);
}

UMaterialExpressionFunctionOutput* FTextureSetMaterialGraphBuilder::CreateOutput(FName Name)
{
	check(!SampleOutputs.Contains(Name));

	UMaterialExpressionFunctionOutput* NewOutput = CreateExpression<UMaterialExpressionFunctionOutput>();
	NewOutput->OutputName = Name;
	SampleOutputs.Add(Name, NewOutput);
	return NewOutput;
}

UMaterialExpressionFunctionOutput* FTextureSetMaterialGraphBuilder::GetOutput(FName Name) const
{
	if (!SampleOutputs.Contains(Name))
		return nullptr;

	return SampleOutputs.FindChecked(Name);
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::MakeConstantParameter(FName Name, FVector4f Default)
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

UMaterialExpression* FTextureSetMaterialGraphBuilder::MakeTextureSamplerCustomNode(int Index)
{
	const FTextureSetPackedTextureDef& TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo& TextureInfo = PackingInfo.GetPackedTextureInfo(Index);
	UMaterialExpressionTextureObjectParameter* TextureObject = PackedTextureObjects[Index];

	UMaterialExpressionTextureStreamingDef* TextureStreamingDef = CreateExpression<UMaterialExpressionTextureStreamingDef>();
	TextureStreamingDef->SamplerType = TextureObject->SamplerType;
	TextureStreamingDef->SamplerSource = ESamplerSourceMode::SSM_Wrap_WorldGroupSettings;// So we don't allocate a sampler
	GetTexcoordStreaming()->ConnectExpression(TextureStreamingDef->GetInput(0), 0);
	TextureObject->ConnectExpression(TextureStreamingDef->GetInput(1), 0);

	UMaterialExpressionCustom* CustomExp = CreateExpression<UMaterialExpressionCustom>();

	CustomExp->IncludeFilePaths.Add("/Engine/Private/Common.ush");

	CustomExp->Inputs.Empty(); // required: class initializes with one input by default
	CustomExp->Inputs.Add({"Texcoord"});
	CustomExp->Inputs.Add({"Tex"});
	CustomExp->Inputs.Add({"DDX"});
	CustomExp->Inputs.Add({"DDY"});

	static const FString ChannelSuffixLower[4] {"r", "g", "b", "a"};
	static const FString ChannelSuffixUpper[4] {"R", "G", "B", "A"};

	CustomExp->Code = "FloatDeriv2 UV = {Texcoord, DDX, DDY};\n";

	// Do the sample and set in the correct size of float for how many channels we have
	CustomExp->Code += FString::Format(TEXT("MaterialFloat{0} Sample = Texture2DSample(Tex, TexSampler, UV)."),
		{(TextureInfo.ChannelCount > 1) ? FString::FromInt(TextureInfo.ChannelCount) : ""});
	
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

	GetTexcoord()->ConnectExpression(CustomExp->GetInput(0), 0);
	TextureStreamingDef->ConnectExpression(CustomExp->GetInput(1), 0);
	GetTexcoordDerivativeX()->ConnectExpression(CustomExp->GetInput(2), 0);
	GetTexcoordDerivativeY()->ConnectExpression(CustomExp->GetInput(3), 0);

	if (RangeCompressMulInput != 0)
	{
		ConstantParameters.FindChecked(TextureInfo.RangeCompressMulName)->ConnectExpression(CustomExp->GetInput(RangeCompressMulInput), 0);
		ConstantParameters.FindChecked(TextureInfo.RangeCompressAddName)->ConnectExpression(CustomExp->GetInput(RangeCompressAddInput), 0);
	}

	CustomExp->Description = FString::Format(TEXT("{0}_Sample"), {Node->GetTextureParameterName(Index).ToString()});

	return CustomExp;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::MakeSynthesizeTangentCustomNode()
{
	HLSLFunctionCallNodeBuilder FunctionCall("SynthesizeTangents", "/Plugin/TextureSets/NormalSynthesis.ush");

	FunctionCall.InArgument("Position", GetPosition(), 0);
	FunctionCall.InArgument("Texcoord", GetRawTexcoord(), 0);
	FunctionCall.InArgument("Normal", GetBaseNormal(), 0);

	FunctionCall.OutArgument("Tangent", ECustomMaterialOutputType::CMOT_Float3);
	FunctionCall.OutArgument("Bitangent", ECustomMaterialOutputType::CMOT_Float3);

	return FunctionCall.Build(*this);
}

HLSLFunctionCallNodeBuilder::HLSLFunctionCallNodeBuilder(FString FunctionName, FString IncludePath)
	: FunctionName(FunctionName)
	, IncludePath(IncludePath)
	, ReturnType(ECustomMaterialOutputType::CMOT_MAX)
{

}

void HLSLFunctionCallNodeBuilder::SetReturnType(ECustomMaterialOutputType NewReturnType)
{
	ReturnType = NewReturnType;
}

void HLSLFunctionCallNodeBuilder::InArgument(FString ArgName, UMaterialExpression* OutExpression, int32 OutIndex)
{
	FunctionArguments.Add(ArgName);
	InputConnections.Add(InputConnection(ArgName, OutExpression, OutIndex));
}

void HLSLFunctionCallNodeBuilder::InArgument(FString ArgName, FString ArgValue)
{
	FunctionArguments.Add(FString::Format(TEXT("/*{0}*/ {1}"), {ArgName, ArgValue}));
}

void HLSLFunctionCallNodeBuilder::OutArgument(FString ArgName, ECustomMaterialOutputType OutType)
{
	FunctionArguments.Add(ArgName);
	Outputs.Add(Output(ArgName, OutType));
}

UMaterialExpression* HLSLFunctionCallNodeBuilder::Build(FTextureSetMaterialGraphBuilder& GraphBuilder)
{
	UMaterialExpressionCustom* CustomExp = GraphBuilder.CreateExpression<UMaterialExpressionCustom>();

	CustomExp->Description = FunctionName;

	CustomExp->Inputs.Empty(); // required: class initializes with one input by default

	for (int i = 0; i < InputConnections.Num(); i++)
	{
		FCustomInput CustomInput;
		CustomInput.InputName = FName(InputConnections[i].Get<FString>());
		CustomExp->Inputs.Add(CustomInput);
	}

	CustomExp->OutputType = ReturnType;

	for (int i = 0; i < Outputs.Num(); i++)
	{
		FCustomOutput CustomInput;
		CustomInput.OutputName = FName(Outputs[i].Get<FString>());
		CustomInput.OutputType = Outputs[i].Get<ECustomMaterialOutputType>();
		CustomExp->AdditionalOutputs.Add(CustomInput);
	}

	CustomExp->RebuildOutputs();

	for (int i = 0; i < InputConnections.Num(); i++)
	{
		UMaterialExpression* SourceConnection = InputConnections[i].Get<UMaterialExpression*>();
		const int32& SourceIndex = InputConnections[i].Get<int32>();
		SourceConnection->ConnectExpression(CustomExp->GetInput(i), SourceIndex);
	}

	CustomExp->IncludeFilePaths.Add(IncludePath);
	CustomExp->Code = FString::Format(TEXT("{0}{1}({2});"),
		{
			ReturnType != ECustomMaterialOutputType::CMOT_MAX ? "return " : "",
			FunctionName,
			FString::Join(FunctionArguments, TEXT(", "))
		});

	// Custom exp nodes always have to return something, so just return 0.
	if (ReturnType == ECustomMaterialOutputType::CMOT_MAX)
		CustomExp->Code += "\nreturn 0;";

	return CustomExp;
}

#endif // WITH_EDITOR