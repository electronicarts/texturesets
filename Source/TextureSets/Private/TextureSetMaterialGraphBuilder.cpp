// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetMaterialGraphBuilder.h"

#if WITH_EDITOR
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialExpressionTextureStreamingDef.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraphBuilder/HLSLFunctionCallNodeBuilder.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDDX.h"
#include "Materials/MaterialExpressionDDY.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureProperty.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVertexTangentWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunction.h"
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
	}

	// Created on demand
	PackedTextureSizes.Init(nullptr, PackingInfo.NumPackedTextures());

	// Call out to modules to do the work of connecting processed texture samples to outputs
	for (const UTextureSetModule* Module : Definition->GetModuleInfo().GetModules())
	{
		Module->ConfigureSamplingGraphBuilder(Node, this);
	}

	// Build all the sub-samples
	TMap<FName, FGraphBuilderOutputAddress> SampleResults = BuildSubsamplesRecursive(FSubSampleAddress::Root());

	// Create outputs for each of the blended subsample results
	for (const auto& [Name, Address] : SampleResults)
	{
		UMaterialExpressionFunctionOutput* Output = CreateOutput(Name);
		Connect(Address, Output, 0);
	}

	SetupFallbackValues();

	// Connect deferred connections
	for (const auto& [Input, Output] : DeferredConnections)
	{
		Output.GetExpression()->ConnectExpression(Input.GetExpression()->GetInput(Input.GetIndex()), Output.GetIndex());
	}
}

const TArray<SubSampleHandle>& FTextureSetMaterialGraphBuilder::AddSubsampleGroup(TArray<FSubSampleDefinition> SampleGroup)
{
	TArray<SubSampleHandle>& SampleGroupHandles = SampleGroups.AddDefaulted_GetRef();

	for (const FSubSampleDefinition& SampleDef : SampleGroup)
	{
		const SubSampleHandle& Handle = SampleGroupHandles.Add_GetRef(SubSampleHandle::NewGuid());
		SubsampleDefinitions.Add(Handle, SampleDef);
	}

	return SampleGroupHandles;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::CreateFunctionCall(FSoftObjectPath FunctionPath)
{
	UMaterialFunction* FunctionObject = Cast<UMaterialFunction>(FunctionPath.ResolveObject());

	if (!FunctionObject)
		FunctionObject = Cast<UMaterialFunction>(FunctionPath.TryLoad());
	
	if (IsValid(FunctionObject))
	{
		UMaterialExpressionMaterialFunctionCall* FunctionCall = CreateExpression<UMaterialExpressionMaterialFunctionCall>();
		FunctionCall->SetMaterialFunction(FunctionObject);
		return FunctionCall;
	}
	
	return nullptr;
}

UMaterialExpressionNamedRerouteDeclaration* FTextureSetMaterialGraphBuilder::CreateReroute(FName Name)
{
	UMaterialExpressionNamedRerouteDeclaration* Reroute = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
	Reroute->Name = Name;
	return Reroute;
}

UMaterialExpressionNamedRerouteDeclaration* FTextureSetMaterialGraphBuilder::CreateReroute(const FString& Name, const FString& Namespace)
{
	return CreateReroute(FName(FString::Format(TEXT("{0}::{1}"), { Namespace, Name })));
}

const FGraphBuilderOutputAddress& FTextureSetMaterialGraphBuilder::GetFallbackValue(EGraphBuilderSharedValueType ValueType)
{
	FGraphBuilderValue& Value = FallbackValues.FindOrAdd(ValueType);

	if (!Value.Reroute.IsValid())
	{
		// First time shared value was requested, create a reroute node for it and use that as the output
		Value.Reroute = FGraphBuilderOutputAddress(CreateReroute(UEnum::GetValueAsName(ValueType)), 0);
	}

	return Value.Reroute;
}

const void FTextureSetMaterialGraphBuilder::SetFallbackValue(FGraphBuilderOutputAddress Address, EGraphBuilderSharedValueType ValueType)
{
	FGraphBuilderValue& Value = FallbackValues.FindOrAdd(ValueType);

	if (ensureMsgf(!Value.Source.IsValid(), TEXT("Shared value has already been set. Multiple overrides are not currently supported")))
		Value.Source = Address;
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

void FTextureSetMaterialGraphBuilder::Connect(UMaterialExpression* OutputNode, FName OutputName, UMaterialExpression* InputNode, FName InputName)
{
	Connect(OutputNode, FindOutputIndexChecked(OutputNode, OutputName), InputNode, FindInputIndexChecked(InputNode, InputName));
}

void FTextureSetMaterialGraphBuilder::Connect(const FGraphBuilderOutputAddress& Output, UMaterialExpression* InputNode, FName InputName)
{
	Connect(Output, InputNode, FindInputIndexChecked(InputNode, InputName));
}

void FTextureSetMaterialGraphBuilder::Connect(UMaterialExpression* OutputNode, FName OutputName, const FGraphBuilderInputAddress& Input)
{
	Connect(OutputNode, FindOutputIndexChecked(OutputNode, OutputName), Input);
}

void FTextureSetMaterialGraphBuilder::Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, UMaterialExpression* InputNode, FName InputName)
{
	Connect(OutputNode, OutputIndex, InputNode, FindInputIndexChecked(InputNode, InputName));
}

void FTextureSetMaterialGraphBuilder::Connect(UMaterialExpression* OutputNode, FName OutputName, UMaterialExpression* InputNode, uint32 InputIndex)
{
	Connect(OutputNode, FindOutputIndexChecked(OutputNode, OutputName), InputNode, InputIndex);
}

void FTextureSetMaterialGraphBuilder::Connect(const FGraphBuilderOutputAddress& Output, const FGraphBuilderInputAddress& Input)
{
	check(Input.IsValid());
	check(Output.IsValid());
	DeferredConnections.Add(Input, Output);
}

FGraphBuilderOutputAddress FTextureSetMaterialGraphBuilder::GetPackedTextureObject(int Index, FGraphBuilderOutputAddress StreamingCoord)
{
	UMaterialExpressionTextureObjectParameter* TextureObjectExpression = PackedTextureObjects[Index];

	// Mat compiler will think there's an extra VT stack if appending streaming def for VT
	// Since it serves no purpose in the VT case, might as well avoid it and avoid confusion
	const FTextureSetPackedTextureDef& TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const bool bVirtualTextureStreaming = TextureDef.bVirtualTextureStreaming;
	if (bVirtualTextureStreaming)
	{
		return FGraphBuilderOutputAddress(TextureObjectExpression, 0);
	}

	if (PackingInfo.GetPackedTextureInfo(Index).Flags & (uint8)ETextureSetTextureFlags::Array)
	{
		// Just do a dummy value for the streaming since it thinks it needs a float3
		UMaterialExpression* AppendNode = CreateExpression<UMaterialExpressionAppendVector>();
		Connect(StreamingCoord, AppendNode, 0);
		UMaterialExpressionConstant* ConstantNode = CreateExpression<UMaterialExpressionConstant>();
		Connect(ConstantNode, 0, AppendNode, 1);
		StreamingCoord = FGraphBuilderOutputAddress(AppendNode, 0);
	}

	UMaterialExpressionTextureStreamingDef* TextureStreamingDef = CreateExpression<UMaterialExpressionTextureStreamingDef>();
	TextureStreamingDef->SamplerType = TextureObjectExpression->SamplerType;
	TextureStreamingDef->SamplerSource = ESamplerSourceMode::SSM_Wrap_WorldGroupSettings;// So we don't allocate a sampler
	Connect(StreamingCoord, TextureStreamingDef, 0);
	Connect(TextureObjectExpression, 0, TextureStreamingDef, 1);

	return FGraphBuilderOutputAddress(TextureStreamingDef, 0);
}

const FGraphBuilderOutputAddress FTextureSetMaterialGraphBuilder::GetPackedTextureSize(int Index)
{
	if (PackedTextureSizes[Index] == nullptr)
	{
		UMaterialExpressionTextureProperty* TextureProp = CreateExpression<UMaterialExpressionTextureProperty>();
		PackedTextureObjects[Index]->ConnectExpression(TextureProp->GetInput(0), 0);
		TextureProp->Property = EMaterialExposedTextureProperty::TMTM_TextureSize;
		PackedTextureSizes[Index] = TextureProp;
	}

	return FGraphBuilderOutputAddress(PackedTextureSizes[Index], 0);
}

const TTuple<FGraphBuilderOutputAddress, FGraphBuilderOutputAddress> FTextureSetMaterialGraphBuilder::GetRangeCompressParams(int Index)
{
	const FTextureSetPackedTextureInfo& TextureInfo = PackingInfo.GetPackedTextureInfo(Index);

	if (TextureInfo.ChannelEncodings & (uint8)ETextureSetChannelEncoding::RangeCompression)
	{
		if (!ConstantParameters.Contains(TextureInfo.RangeCompressMulName))
			MakeConstantParameter(TextureInfo.RangeCompressAddName, FVector4f::Zero());

		if (!ConstantParameters.Contains(TextureInfo.RangeCompressMulName))
			MakeConstantParameter(TextureInfo.RangeCompressMulName, FVector4f::One());

		return TTuple<FGraphBuilderOutputAddress, FGraphBuilderOutputAddress>(
			FGraphBuilderOutputAddress(ConstantParameters.FindChecked(TextureInfo.RangeCompressMulName), 0),
			FGraphBuilderOutputAddress(ConstantParameters.FindChecked(TextureInfo.RangeCompressAddName), 0));
	}
	else
	{
		UMaterialExpressionConstant4Vector* One = CreateExpression<UMaterialExpressionConstant4Vector>();
		One->Constant = FLinearColor::White;
		UMaterialExpressionConstant4Vector* Zero = CreateExpression<UMaterialExpressionConstant4Vector>();
		Zero->Constant = FLinearColor::Black;

		return TTuple<FGraphBuilderOutputAddress, FGraphBuilderOutputAddress>(
			FGraphBuilderOutputAddress(One, 0),
			FGraphBuilderOutputAddress(Zero, 0));
	}
}

void FTextureSetMaterialGraphBuilder::AddSampleBuilder(SampleBuilderFunction SampleBuilder)
{
	SampleBuilders.Add(SampleBuilder);
}

UMaterialExpressionFunctionInput* FTextureSetMaterialGraphBuilder::CreateInput(FName Name, EFunctionInputType Type, EInputSort Sort)
{
	if (GraphInputs.Contains(Name))
		LogError(FText::Format(INVTEXT("Input {0} already exists; an input can only be created once."), FText::FromName(Name)));

	UMaterialExpressionFunctionInput* NewInput = CreateExpression<UMaterialExpressionFunctionInput>();
	NewInput->InputName = Name;
	NewInput->InputType = Type;
	NewInput->SortPriority = (int)Sort;
	GraphInputs.Add(Name, NewInput);
	return NewInput;
}

UMaterialExpressionFunctionOutput* FTextureSetMaterialGraphBuilder::CreateOutput(FName Name)
{
	if (GraphOutputs.Contains(Name))
		LogError(FText::Format(INVTEXT("Output {0} already exists; an input can only be created once."), FText::FromName(Name)));

	UMaterialExpressionFunctionOutput* NewOutput = CreateExpression<UMaterialExpressionFunctionOutput>();
	NewOutput->OutputName = Name;
	GraphOutputs.Add(Name, NewOutput);
	return NewOutput;
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

TMap<FName, FGraphBuilderOutputAddress> FTextureSetMaterialGraphBuilder::BuildSubsamplesRecursive(const FSubSampleAddress& Address)
{
	const int32 Depth = Address.GetHandleChain().Num();
	check(Depth <= SampleGroups.Num());
	const bool IsLeaf = Depth == SampleGroups.Num();

	if (IsLeaf)
	{
		FTextureSetSubsampleContext Context(this, Address);

		for (SampleBuilderFunction SampleBuilder : SampleBuilders)
		{
			SampleBuilder(Context);
		}

		SetupTextureValues(Context);
		SetupSharedValues(Context);

		return Context.Results;
	}
	else
	{
		const TArray<SubSampleHandle>& SampleGroup = SampleGroups[Depth];

		TArray<TMap<FName, FGraphBuilderOutputAddress>> Results;

		for (SubSampleHandle SampleHandle : SampleGroup)
		{
			Results.Add(BuildSubsamplesRecursive(FSubSampleAddress(Address, SampleHandle)));
		}

		return BlendSubsampleResults(Address, Results);
	}
}

TMap<FName, FGraphBuilderOutputAddress> FTextureSetMaterialGraphBuilder::BlendSubsampleResults(const FSubSampleAddress& Address, TArray<TMap<FName, FGraphBuilderOutputAddress>> Results)
{
	TMap<FName, FGraphBuilderOutputAddress> BlendedResult;

	// Validate all results share the same elements
	for (const auto& [Name, Output] : Results[0])
	{
		for (int i = 1; i < Results.Num(); i++)
			check(Results[i].Contains(Name));
	}

	// For each set of results
	for (int i = 0; i < Results.Num(); i++)
	{
		const FGraphBuilderOutputAddress& SampleWeight = SubsampleDefinitions.FindChecked(SampleGroups[Address.GetHandleChain().Num()][i]).Weight;

		for (const auto& [Name, Result] : Results[i])
		{
			// Multiply each result by the sample weight
			UMaterialExpressionMultiply* Mul = CreateExpression<UMaterialExpressionMultiply>();
			Connect(Result, Mul, 0);
			Connect(SampleWeight, Mul, 1);

			if (FGraphBuilderOutputAddress* PrevBlended = BlendedResult.Find(Name))
			{
				// Add the weighted results together
				UMaterialExpressionAdd* Add = CreateExpression<UMaterialExpressionAdd>();
				Connect(*PrevBlended, Add, 0);
				Connect(FGraphBuilderOutputAddress(Mul, 0), Add, 1);
				BlendedResult.Add(Name, FGraphBuilderOutputAddress(Add, 0));
			}
			else
			{
				BlendedResult.Add(Name, FGraphBuilderOutputAddress(Mul, 0));
			}
		}
	}

	return BlendedResult;
}

void FTextureSetMaterialGraphBuilder::SetupFallbackValues()
{
	// Hook up proper inputs to any reroute nodes that were used.
	// Note: Order here is important, as we have some dependencies

	const auto NeedsValue = [this](EGraphBuilderSharedValueType ValueType) {
		FGraphBuilderValue* Value = FallbackValues.Find(ValueType);
		return Value != nullptr && Value->Reroute.IsValid() && !Value->Source.IsValid();
	};

	const bool NeedsTangent = NeedsValue(EGraphBuilderSharedValueType::Tangent);
	const bool NeedsBitangent = NeedsValue(EGraphBuilderSharedValueType::Bitangent);

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
			UMaterialExpression* SynthesizeTangentNode = MakeSynthesizeTangentCustomNode(
				GetFallbackValue(EGraphBuilderSharedValueType::Position),
				GetFallbackValue(EGraphBuilderSharedValueType::Texcoord_Raw),
				GetFallbackValue(EGraphBuilderSharedValueType::BaseNormal));

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
			SetFallbackValue(TangentSource, EGraphBuilderSharedValueType::Tangent);

		if (NeedsBitangent)
			SetFallbackValue(BitangentSource, EGraphBuilderSharedValueType::Bitangent);
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

		SetFallbackValue(BaseNormalSource, EGraphBuilderSharedValueType::BaseNormal);
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

		SetFallbackValue(CameraVectorSource, EGraphBuilderSharedValueType::CameraVector);
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

		SetFallbackValue(PositionSource, EGraphBuilderSharedValueType::Position);
	}
	
	if (NeedsValue(EGraphBuilderSharedValueType::Texcoord_Streaming))
	{
		SetFallbackValue(GetFallbackValue(EGraphBuilderSharedValueType::Texcoord_Sampling), EGraphBuilderSharedValueType::Texcoord_Streaming);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::Texcoord_DDX))
	{
		UMaterialExpression* DDX = CreateExpression<UMaterialExpressionDDX>();
		Connect(GetFallbackValue(EGraphBuilderSharedValueType::Texcoord_Mip), FGraphBuilderInputAddress(DDX, 0));
		SetFallbackValue(FGraphBuilderOutputAddress(DDX, 0), EGraphBuilderSharedValueType::Texcoord_DDX);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::Texcoord_DDY))
	{
		UMaterialExpression* DDY = CreateExpression<UMaterialExpressionDDY>();
		Connect(GetFallbackValue(EGraphBuilderSharedValueType::Texcoord_Mip), FGraphBuilderInputAddress(DDY, 0));
		SetFallbackValue(FGraphBuilderOutputAddress(DDY, 0), EGraphBuilderSharedValueType::Texcoord_DDY);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::Texcoord_Mip))
	{
		SetFallbackValue(GetFallbackValue(EGraphBuilderSharedValueType::Texcoord_Sampling), EGraphBuilderSharedValueType::Texcoord_Mip);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::Texcoord_Sampling))
	{
		SetFallbackValue(GetFallbackValue(EGraphBuilderSharedValueType::Texcoord_Raw), EGraphBuilderSharedValueType::Texcoord_Sampling);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::Texcoord_Raw))
	{
		UMaterialExpression* UVInput = CreateInput("UV", EFunctionInputType::FunctionInput_Vector2, EInputSort::UV);
		SetFallbackValue(FGraphBuilderOutputAddress(UVInput, 0), EGraphBuilderSharedValueType::Texcoord_Raw);
	}

	if (NeedsValue(EGraphBuilderSharedValueType::ArrayIndex))
	{
		FGraphBuilderOutputAddress IndexInput(CreateInput("Array Index", EFunctionInputType::FunctionInput_Scalar, EInputSort::UV), 0);
		SetFallbackValue(IndexInput, EGraphBuilderSharedValueType::ArrayIndex);
	}

	// Link all shared value sources to the reroute nodes
	for (auto& [Type, Value] : FallbackValues)
	{			
		check(Value.Source.IsValid());
		check(Value.Reroute.IsValid());
		Connect(Value.Source, Value.Reroute.GetExpression(), 0);
	}
}

void FTextureSetMaterialGraphBuilder::SetupSharedValues(FTextureSetSubsampleContext& Context)
{
	for (auto& [Type, Value] : Context.SubsampleValues)
	{
		// Fall back to shared value if this subsample value hasn't been set explicitly
		const FGraphBuilderOutputAddress& SourceAddress = Value.Source.IsValid() ? Value.Source : GetFallbackValue(Type);

		check(SourceAddress.IsValid());
		if (Value.Reroute.IsValid())
			Connect(SourceAddress, Value.Reroute.GetExpression(), 0);
	}
}

void FTextureSetMaterialGraphBuilder::SetupTextureValues(FTextureSetSubsampleContext& Context)
{
	TMap<FName, FGraphBuilderValue>& TextureValues = Context.ProcessedTextureValues;

	// Sample each packed texture
	for (int t = 0; t < PackingInfo.NumPackedTextures(); t++)
	{
		UMaterialExpression* TextureSample = MakeTextureSamplerCustomNode(t, Context);

		const TArray<FName> SourceChannels = PackingInfo.GetPackedTextureDef(t).GetSources();
		for (int c = 0; c < SourceChannels.Num(); c++)
		{
			if (SourceChannels[c].IsNone())
				continue;

			FGraphBuilderValue& Value = TextureValues.FindOrAdd(SourceChannels[c]);
			Value.Source = FGraphBuilderOutputAddress(TextureSample, c + 1);
		}
	}

	// Append vectors back into their equivalent processed map samples
	for (const auto& [TextureName, ProcessedTexture] : ModuleInfo.GetProcessedTextures())
	{
		FGraphBuilderOutputAddress WorkingAddress = TextureValues.FindChecked(FName(TextureName.ToString() + TextureSetsHelpers::ChannelSuffixes[0])).Source;

		for (int i = 1; i < ProcessedTexture.ChannelCount; i++)
		{
			UMaterialExpression* AppendNode = CreateExpression<UMaterialExpressionAppendVector>();
			FGraphBuilderOutputAddress NextChannel = TextureValues.FindChecked(FName(TextureName.ToString() + TextureSetsHelpers::ChannelSuffixes[i])).Source;
			Connect(WorkingAddress, AppendNode, 0);
			Connect(NextChannel, AppendNode, 1);
			WorkingAddress = FGraphBuilderOutputAddress(AppendNode, 0);
		}

		FGraphBuilderValue& Value = TextureValues.FindOrAdd(TextureName);
		Value.Source = WorkingAddress;
	}

	// Link all shared value sources to the reroute nodes
	for (auto& [Type, Value] : TextureValues)
	{			
		check(Value.Source.IsValid());
		if (Value.Reroute.IsValid())
			Connect(Value.Source, Value.Reroute.GetExpression(), 0);
	}
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::MakeTextureSamplerCustomNode(int Index, FTextureSetSubsampleContext& Context)
{
	const FTextureSetPackedTextureDef& TextureDef = PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo& TextureInfo = PackingInfo.GetPackedTextureInfo(Index);

	FGraphBuilderOutputAddress TextureObject = GetPackedTextureObject(
		Index, 
		Context.GetSharedValue(EGraphBuilderSharedValueType::Texcoord_Streaming));

	FGraphBuilderOutputAddress SampleCoord = Context.GetSharedValue(EGraphBuilderSharedValueType::Texcoord_Sampling);
	FGraphBuilderOutputAddress DDX = Context.GetSharedValue(EGraphBuilderSharedValueType::Texcoord_DDX);
	FGraphBuilderOutputAddress DDY = Context.GetSharedValue(EGraphBuilderSharedValueType::Texcoord_DDY);

	// Append the array index to out UV to get the sample coordinate
	if (TextureInfo.Flags & (uint8)ETextureSetTextureFlags::Array)
	{
		UMaterialExpression* AppendNode = CreateExpression<UMaterialExpressionAppendVector>();
		Connect(SampleCoord, AppendNode, 0);
		Connect(Context.GetSharedValue(EGraphBuilderSharedValueType::ArrayIndex), AppendNode, 1);
		SampleCoord = FGraphBuilderOutputAddress(AppendNode, 0);
		DDX = FGraphBuilderOutputAddress(AppendNode, 0);
		DDY = FGraphBuilderOutputAddress(AppendNode, 0);

	}

	static const FString ChannelSuffixLower[4] {"r", "g", "b", "a"};
	static const FString ChannelSuffixUpper[4] {"R", "G", "B", "A"};

	const bool bVirtualTextureStreaming = TextureDef.bVirtualTextureStreaming;

	UMaterialExpressionCustom* CustomExp = CreateExpression<UMaterialExpressionCustom>();
	CustomExp->Inputs.Empty(); // required: class initializes with one input by default
	CustomExp->IncludeFilePaths.Add("/Engine/Private/Common.ush");
	CustomExp->Code = "";

	FString SampleType = FString::Format(TEXT("MaterialFloat{0} Sample = "), {(TextureInfo.ChannelCount > 1) ? FString::FromInt(TextureInfo.ChannelCount) : ""});
	
	if (bVirtualTextureStreaming)
	{
		TObjectPtr<UMaterialExpressionTextureSample> SampleExpression = CreateExpression<UMaterialExpressionTextureSample>();
		SampleExpression->MipValueMode = TMVM_Derivative;

		Connect(TextureObject, SampleExpression, "TextureObject");
		Connect(DDX, SampleExpression, "DDX(UVs)");
		Connect(DDY, SampleExpression, "DDY(UVs)");
		Connect(SampleCoord, SampleExpression, "Coordinates");

		CustomExp->Inputs.Add({"InSample"});

		constexpr uint32 RGBAOutputIndex = 5;
		Connect(FGraphBuilderOutputAddress(SampleExpression, RGBAOutputIndex), CustomExp, 0);

		// Set the correct size of float for how many channels we have
 		CustomExp->Code += FString::Format(TEXT("{0} InSample."), {SampleType});
	}
	else
	{
		CustomExp->Inputs.Add({"Texcoord"});
		CustomExp->Inputs.Add({"Tex"});
		CustomExp->Inputs.Add({"DDX"});
		CustomExp->Inputs.Add({"DDY"});

		const bool bIsArray = (TextureInfo.Flags & (uint8)ETextureSetTextureFlags::Array);

		if (!bIsArray)
			CustomExp->Code += "FloatDeriv2 UV = {Texcoord.xy, DDX, DDY};\n";

		// Set the correct size of float for how many channels we have
		CustomExp->Code += FString::Format(TEXT("{0}"), {SampleType});

		// Do the appropriate sample
		if (bIsArray)
		{
			CustomExp->Code += TEXT("Texture2DArraySampleGrad(Tex, TexSampler, Texcoord.xyz, DDX, DDY).");
		}
		else
		{
			CustomExp->Code += TEXT("Texture2DSample(Tex, TexSampler, UV).");
		}
	}
	
	for (int c = 0; c < TextureInfo.ChannelCount; c++)
		CustomExp->Code += ChannelSuffixLower[c];

	CustomExp->Code += ";\n";

	uint32 RangeCompressMulInput = 0;
	uint32 RangeCompressAddInput = 0;

	// Process each channel
	for (int c = 0; c < TextureInfo.ChannelCount; c++)
	{
		TArray<FStringFormatArg> FormatArgs = {ChannelSuffixLower[c], ChannelSuffixUpper[c]};

		if ((TextureInfo.ChannelInfo[c].ChannelEncoding & (uint8)ETextureSetChannelEncoding::SRGB) && (!TextureInfo.HardwareSRGB || c >= 3))
		{
			// Need to do sRGB decompression in the shader
			CustomExp->Code += FString::Format(TEXT("Sample.{0} = pow(Sample.{0}, 2.2f);\n"), FormatArgs);
		}

		if (TextureInfo.ChannelInfo[c].ChannelEncoding & (uint8)ETextureSetChannelEncoding::RangeCompression)
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

		// Add an output pin for this channel
		CustomExp->AdditionalOutputs.Add(FCustomOutput({FName(ChannelSuffixUpper[c]), ECustomMaterialOutputType::CMOT_Float1}));
		CustomExp->Code += FString::Format(TEXT("{1} = Sample.{0};\n"), FormatArgs);
	}

	// Return the correct output type
	static const ECustomMaterialOutputType OutputTypes[4] {CMOT_Float1, CMOT_Float2, CMOT_Float3, CMOT_Float4};
	CustomExp->OutputType = OutputTypes[TextureInfo.ChannelCount - 1];
	CustomExp->Code += "return Sample;";

	CustomExp->RebuildOutputs();

	if (!bVirtualTextureStreaming)
	{
		Connect(SampleCoord, CustomExp, 0);
		Connect(TextureObject, CustomExp, 1);
		Connect(DDX, CustomExp, 2);
		Connect(DDY, CustomExp, 3);
	}

	if (RangeCompressMulInput != 0)
	{
		auto [Mul, Add] = GetRangeCompressParams(Index);

		Connect(Mul, (UMaterialExpression*)CustomExp, RangeCompressMulInput);
		Connect(Add, (UMaterialExpression*)CustomExp, RangeCompressAddInput);
	}

	CustomExp->Description = FString::Format(TEXT("{0}_Sample"), {Node->GetTextureParameterName(Index).ToString()});

	return CustomExp;
}

UMaterialExpression* FTextureSetMaterialGraphBuilder::MakeSynthesizeTangentCustomNode(const FGraphBuilderOutputAddress& Position, const FGraphBuilderOutputAddress& Texcoord, const FGraphBuilderOutputAddress& Normal)
{
	HLSLFunctionCallNodeBuilder FunctionCall("SynthesizeTangents", "/Plugin/TextureSets/NormalSynthesis.ush");

	FunctionCall.InArgument("Position", Position);
	FunctionCall.InArgument("Texcoord", Texcoord);
	FunctionCall.InArgument("Normal", Normal);

	FunctionCall.OutArgument("Tangent", ECustomMaterialOutputType::CMOT_Float3);
	FunctionCall.OutArgument("Bitangent", ECustomMaterialOutputType::CMOT_Float3);

	return FunctionCall.Build(this);
}

int32 FTextureSetMaterialGraphBuilder::FindInputIndexChecked(UMaterialExpression* InputNode, FName InputName)
{
	const int32 NumInputs = InputNode->GetInputs().Num();

	int32 InputIndex = -1;
	for (int32 Idx = 0; Idx < NumInputs; Idx++)
	{
		if (InputNode->GetInputName(Idx).Compare(InputName) == 0)
		{
			InputIndex = Idx;
			break;
		}
	}
	checkf(InputIndex >= 0, TEXT("Could not find input with name {0}"), InputName);
	return InputIndex;
}

int32 FTextureSetMaterialGraphBuilder::FindOutputIndexChecked(UMaterialExpression* OutputNode, FName OutputName)
{
	const TArray<FExpressionOutput>& Outputs = OutputNode->GetOutputs();
	
	int32 OutputIndex = -1;
	for (int i = 0; i < Outputs.Num(); i++)
	{
		if (Outputs[i].OutputName == OutputName)
		{
			OutputIndex = i;
			break;
		}
	}
	checkf(OutputIndex >= 0, TEXT("Could not find output with name {0}"), OutputName);
	return OutputIndex;
}

FString FTextureSetMaterialGraphBuilder::SubsampleAddressToString(const FSubSampleAddress& Address)
{
	if (Address.GetHandleChain().Num() == 0)
		return "Root";
	else
	{
		FString Result = SubsampleDefinitions.FindChecked(Address.GetHandleChain()[0]).Name.ToString();
		for (int i = 1; i < Address.GetHandleChain().Num(); i++)
		{
			Result += "::" + SubsampleDefinitions.FindChecked(Address.GetHandleChain()[i]).Name.ToString();
		}
		return Result;
	}
}
#endif // WITH_EDITOR