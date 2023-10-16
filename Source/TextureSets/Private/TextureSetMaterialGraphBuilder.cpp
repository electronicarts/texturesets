// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetMaterialGraphBuilder.h"

#if WITH_EDITOR
#include "MaterialGraphBuilder/HLSLFunctionCallNodeBuilder.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialExpressionTextureStreamingDef.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionConstant.h"
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
	, RawUV(CreateInput("UV", EFunctionInputType::FunctionInput_Vector2, EInputSort::UV), 0)
{

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

	SetupSharedValues();

	// Connect deferred connections
	for (const auto& [Input, Output] : DeferredConnections)
	{
		check(Input.IsValid());
		check(Output.IsValid());
		Output.GetExpression()->ConnectExpression(Input.GetExpression()->GetInput(Input.GetIndex()), Output.GetIndex());
	}
}

const FGraphBuilderOutputAddress& FTextureSetMaterialGraphBuilder::GetSharedValue(EGraphBuilderSharedValueType Value)
{
	FGraphBuilderOutputAddress* Address = SharedValueReroute.Find(Value);

	if (Address)
	{
		return *Address;
	}
	else
	{
		// First time shared value was requested, create a reroute node for it and use that as the output
		UMaterialExpressionNamedRerouteDeclaration* Reroute = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
		Reroute->Name = UEnum::GetValueAsName(Value);
		FGraphBuilderOutputAddress NewAddress(Reroute, 0);
		return SharedValueReroute.Add(Value, NewAddress);
	}
}

const void FTextureSetMaterialGraphBuilder::SetSharedValue(FGraphBuilderOutputAddress Address, EGraphBuilderSharedValueType Value)
{
	if (ensureMsgf(!SharedValueSources.Contains(Value), TEXT("Standard shared value has already been set. Multiple overrides are not currently supported")))
		SharedValueSources.Add(Value, Address);
}

void FTextureSetMaterialGraphBuilder::Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, UMaterialExpression* InputNode, uint32 InputIndex)
{
	Connect(FGraphBuilderOutputAddress(OutputNode, OutputIndex), FGraphBuilderInputAddress(InputNode, InputIndex));
}

void FTextureSetMaterialGraphBuilder::Connect(const FGraphBuilderOutputAddress& Output, UMaterialExpression* InputNode, uint32 InputIndex)
{
	Connect(Output, FGraphBuilderInputAddress(InputNode, InputIndex));
}

void FTextureSetMaterialGraphBuilder::Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, const FGraphBuilderInputAddress& Input)
{
	Connect(FGraphBuilderOutputAddress(OutputNode, OutputIndex), Input);
}

void FTextureSetMaterialGraphBuilder::Connect(const FGraphBuilderOutputAddress& Output, const FGraphBuilderInputAddress& Input)
{
	DeferredConnections.Add(Input, Output);
}

const FGraphBuilderOutputAddress& FTextureSetMaterialGraphBuilder::GetRawUV() const
{
	return RawUV;
}

const FGraphBuilderOutputAddress FTextureSetMaterialGraphBuilder::GetProcessedTextureSample(FName Name)
{
	return FGraphBuilderOutputAddress(ProcessedTextureSamples.FindChecked(Name), 0);
}

UMaterialExpressionTextureObjectParameter* FTextureSetMaterialGraphBuilder::GetPackedTextureObject(int Index)
{
	return PackedTextureObjects[Index];
}

const FGraphBuilderOutputAddress FTextureSetMaterialGraphBuilder::GetPackedTextureSize(int Index)
{
	if (PackedTextureSizes[Index] == nullptr)
	{
		UMaterialExpressionTextureProperty* TextureProp = CreateExpression<UMaterialExpressionTextureProperty>();
		GetPackedTextureObject(Index)->ConnectExpression(TextureProp->GetInput(0), 0);
		TextureProp->Property = EMaterialExposedTextureProperty::TMTM_TextureSize;
		PackedTextureSizes[Index] = TextureProp;
	}

	return FGraphBuilderOutputAddress(PackedTextureSizes[Index], 0);
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

void FTextureSetMaterialGraphBuilder::SetupSharedValues()
{
	// Hook up proper inputs to any reroute nodes that were used.
	// Note: Order here is important, as we have some dependencies

	const auto NeedsValue = [this](EGraphBuilderSharedValueType Type) {
		return SharedValueReroute.Contains(Type) && !SharedValueSources.Contains(Type);
	};

	const bool NeedsTangent = NeedsValue(EGraphBuilderSharedValueType::Tangent);
	const bool NeedsBitangent = NeedsValue(EGraphBuilderSharedValueType::BiTangent);

	if (NeedsTangent || NeedsBitangent)
	{
		FGraphBuilderOutputAddress TangentSource;
		FGraphBuilderOutputAddress BitangentSource;
		switch (Node->TangentSource)
		{
		case ETangentSource::Explicit:
		{
			TangentSource = FGraphBuilderOutputAddress(CreateInput("Tangent", EFunctionInputType::FunctionInput_Vector3, EInputSort::Tangent), 0);
			BitangentSource = FGraphBuilderOutputAddress(CreateInput("Bitangent", EFunctionInputType::FunctionInput_Vector3, EInputSort::Tangent), 0);
			break;
		}
		case ETangentSource::Synthesized:
		{
			UMaterialExpression* SynthesizeTangentNode = MakeSynthesizeTangentCustomNode();
			TangentSource = FGraphBuilderOutputAddress(SynthesizeTangentNode, 1);
			BitangentSource = FGraphBuilderOutputAddress(SynthesizeTangentNode, 2);
			break;
		}
		case ETangentSource::Vertex:
		{
			TangentSource = FGraphBuilderOutputAddress(CreateExpression<UMaterialExpressionVertexTangentWS>(), 0);

			if (NeedsBitangent)
			{
				// Don't have a node explicitly for the vertex bitangent, so we create it by transforming a vector from tangent to world space
				UMaterialExpressionConstant3Vector* BitangentConstant = CreateExpression<UMaterialExpressionConstant3Vector>();
				BitangentConstant->Constant = FLinearColor(0, 1, 0);
				UMaterialExpressionTransform* TransformBitangent = CreateExpression<UMaterialExpressionTransform>();
				TransformBitangent->TransformSourceType = EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_Tangent;
				TransformBitangent->TransformType = EMaterialVectorCoordTransform::TRANSFORM_World;
				Connect(BitangentConstant, 0, TransformBitangent, 0);
				BitangentSource = FGraphBuilderOutputAddress(TransformBitangent, 0);
			}
			break;
		}
		default:
			unimplemented();
			break;
		}

		if (NeedsTangent)
			SetSharedValue(TangentSource, EGraphBuilderSharedValueType::Tangent);

		if (NeedsBitangent)
			SetSharedValue(BitangentSource, EGraphBuilderSharedValueType::BiTangent);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::BaseNormal))
	{
		FGraphBuilderOutputAddress BaseNormalSource;

		switch (Node->BaseNormalSource)
		{
		case EBaseNormalSource::Explicit:
			BaseNormalSource = FGraphBuilderOutputAddress(CreateInput("Base Normal", EFunctionInputType::FunctionInput_Vector3, EInputSort::BaseNormal), 0);
			break;
		case EBaseNormalSource::Vertex:
			BaseNormalSource = FGraphBuilderOutputAddress(CreateExpression<UMaterialExpressionVertexNormalWS>(), 0);
			break;
		default:
			unimplemented();
			break;
		}

		SetSharedValue(BaseNormalSource, EGraphBuilderSharedValueType::BaseNormal);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::CameraVector))
	{
		FGraphBuilderOutputAddress CameraVectorSource;

		switch (Node->CameraVectorSource)
		{
		case ECameraVectorSource::Explicit:
			CameraVectorSource = FGraphBuilderOutputAddress(CreateInput("Camera Vector", EFunctionInputType::FunctionInput_Vector3, EInputSort::CameraVector), 0);
			break;
		case ECameraVectorSource::World:
			CameraVectorSource = FGraphBuilderOutputAddress(CreateExpression<UMaterialExpressionCameraVectorWS>(), 0);
			break;
		default:
			unimplemented();
			break;
		}

		SetSharedValue(CameraVectorSource, EGraphBuilderSharedValueType::CameraVector);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::Position))
	{
		FGraphBuilderOutputAddress PositionSource;

		switch (Node->PositionSource)
		{
		case EPositionSource::Explicit:
			PositionSource = FGraphBuilderOutputAddress(CreateInput("Position", EFunctionInputType::FunctionInput_Vector3, EInputSort::Position), 0);
			break;
		case EPositionSource::World:
			PositionSource = FGraphBuilderOutputAddress(CreateExpression<UMaterialExpressionWorldPosition>(), 0);
			break;
		default:
			unimplemented();
			break;
		}

		SetSharedValue(PositionSource, EGraphBuilderSharedValueType::Position);
	}
	
	if (NeedsValue(EGraphBuilderSharedValueType::UV_Streaming))
	{
		SetSharedValue(GetSharedValue(EGraphBuilderSharedValueType::UV_Sampling), EGraphBuilderSharedValueType::UV_Streaming);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::UV_DDX))
	{
		UMaterialExpression* DDX = CreateExpression<UMaterialExpressionDDX>();
		Connect(GetSharedValue(EGraphBuilderSharedValueType::UV_Mip), FGraphBuilderInputAddress(DDX, 0));
		SetSharedValue(FGraphBuilderOutputAddress(DDX, 0), EGraphBuilderSharedValueType::UV_DDX);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::UV_DDY))
	{
		UMaterialExpression* DDY = CreateExpression<UMaterialExpressionDDY>();
		Connect(GetSharedValue(EGraphBuilderSharedValueType::UV_Mip), FGraphBuilderInputAddress(DDY, 0));
		SetSharedValue(FGraphBuilderOutputAddress(DDY, 0), EGraphBuilderSharedValueType::UV_DDY);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::UV_Mip))
	{
		SetSharedValue(GetSharedValue(EGraphBuilderSharedValueType::UV_Sampling), EGraphBuilderSharedValueType::UV_Mip);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::UV_Sampling))
	{
		SetSharedValue(RawUV, EGraphBuilderSharedValueType::UV_Sampling);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::ArrayIndex))
	{
		FGraphBuilderOutputAddress IndexInput(CreateInput("Array Index", EFunctionInputType::FunctionInput_Scalar, EInputSort::UV), 0);
		SetSharedValue(IndexInput, EGraphBuilderSharedValueType::ArrayIndex);
	}

	// Link all shared value sources to the reroute nodes
	for (auto& [Type, RerouteOutput] : SharedValueReroute)
	{
		FGraphBuilderOutputAddress ValueSource = SharedValueSources.FindChecked(Type);
		check(RerouteOutput.IsValid());
		FGraphBuilderInputAddress RerouteInput = FGraphBuilderInputAddress(RerouteOutput.GetExpression(), 0);

		Connect(ValueSource, RerouteInput);
	}
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::MakeTextureSamplerCustomNode(int Index)
{
	const FTextureSetPackedTextureDef& TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo& TextureInfo = PackingInfo.GetPackedTextureInfo(Index);
	UMaterialExpressionTextureObjectParameter* TextureObject = PackedTextureObjects[Index];

	FGraphBuilderOutputAddress SampleCoord = GetSharedValue(EGraphBuilderSharedValueType::UV_Sampling);
	FGraphBuilderOutputAddress StreamingCoord = GetSharedValue(EGraphBuilderSharedValueType::UV_Streaming);

	if (EnumHasAnyFlags(TextureInfo.Flags, ETextureSetTextureFlags::Array))
	{
		// Append the array index to out UV to get the sample coordinate
		{
			UMaterialExpression* AppendNode = CreateExpression<UMaterialExpressionAppendVector>();
			Connect(SampleCoord, AppendNode, 0);
			Connect(GetSharedValue(EGraphBuilderSharedValueType::ArrayIndex), AppendNode, 1);
			SampleCoord = FGraphBuilderOutputAddress(AppendNode, 0);
		}

		// Just do a dummy value for the streaming since it thinks it needs a float3
		{
			UMaterialExpression* AppendNode = CreateExpression<UMaterialExpressionAppendVector>();
			Connect(StreamingCoord, AppendNode, 0);
			UMaterialExpressionConstant* ConstantNode = CreateExpression<UMaterialExpressionConstant>();
			Connect(ConstantNode, 0, AppendNode, 1);
			StreamingCoord = FGraphBuilderOutputAddress(AppendNode, 0);
		}
	}

	UMaterialExpressionTextureStreamingDef* TextureStreamingDef = CreateExpression<UMaterialExpressionTextureStreamingDef>();
	TextureStreamingDef->SamplerType = TextureObject->SamplerType;
	TextureStreamingDef->SamplerSource = ESamplerSourceMode::SSM_Wrap_WorldGroupSettings;// So we don't allocate a sampler
	Connect(StreamingCoord, TextureStreamingDef, 0);
	Connect(TextureObject, 0, TextureStreamingDef, 1);

	UMaterialExpressionCustom* CustomExp = CreateExpression<UMaterialExpressionCustom>();

	CustomExp->IncludeFilePaths.Add("/Engine/Private/Common.ush");

	CustomExp->Inputs.Empty(); // required: class initializes with one input by default
	CustomExp->Inputs.Add({"Texcoord"});
	CustomExp->Inputs.Add({"Tex"});
	CustomExp->Inputs.Add({"DDX"});
	CustomExp->Inputs.Add({"DDY"});

	static const FString ChannelSuffixLower[4] {"r", "g", "b", "a"};
	static const FString ChannelSuffixUpper[4] {"R", "G", "B", "A"};
	
	CustomExp->Code = "";

	if (!EnumHasAnyFlags(TextureInfo.Flags, ETextureSetTextureFlags::Array))
		CustomExp->Code += "FloatDeriv2 UV = {Texcoord.xy, DDX, DDY};\n";

	// Set the correct size of float for how many channels we have
	CustomExp->Code += FString::Format(TEXT("MaterialFloat{0} Sample = "),
		{(TextureInfo.ChannelCount > 1) ? FString::FromInt(TextureInfo.ChannelCount) : ""});

	// Do the appropriate sample
	if (EnumHasAnyFlags(TextureInfo.Flags, ETextureSetTextureFlags::Array))
	{
		CustomExp->Code += TEXT("Texture2DArraySampleGrad(Tex, TexSampler, Texcoord.xyz, DDX, DDY).");
	}
	else
	{
		CustomExp->Code += TEXT("Texture2DSample(Tex, TexSampler, UV).");
	}
	
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

	Connect(SampleCoord, CustomExp, 0);
	Connect(TextureStreamingDef, 0, CustomExp, 1);
	Connect(GetSharedValue(EGraphBuilderSharedValueType::UV_DDX), CustomExp, 2);
	Connect(GetSharedValue(EGraphBuilderSharedValueType::UV_DDY), CustomExp, 3);

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

	FunctionCall.InArgument("Position", GetSharedValue(EGraphBuilderSharedValueType::Position));
	FunctionCall.InArgument("Texcoord", RawUV);
	FunctionCall.InArgument("Normal", GetSharedValue(EGraphBuilderSharedValueType::BaseNormal));

	FunctionCall.OutArgument("Tangent", ECustomMaterialOutputType::CMOT_Float3);
	FunctionCall.OutArgument("Bitangent", ECustomMaterialOutputType::CMOT_Float3);

	return FunctionCall.Build(*this);
}
#endif // WITH_EDITOR
