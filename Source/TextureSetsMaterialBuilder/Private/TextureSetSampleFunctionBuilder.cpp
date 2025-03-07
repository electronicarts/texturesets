// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetSampleFunctionBuilder.h"

#include "MaterialExpressionTextureStreamingDef.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "HLSLFunctionCallNodeBuilder.h"
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
#include "TextureSetModule.h"
#include "TextureSetsHelpers.h"

FTextureSetSampleFunctionBuilder::FTextureSetSampleFunctionBuilder(const FTextureSetMaterialGraphBuilderArgs& Args)
	: Args(Args)
{
	// Create Texture Parameters for each packed texture
	for (int i = 0; i < Args.PackingInfo.NumPackedTextures(); i++)
	{
		const FTextureSetPackedTextureDef& TextureDef = Args.PackingInfo.GetPackedTextureDef(i);
		const FTextureSetPackedTextureInfo& TextureInfo = Args.PackingInfo.GetPackedTextureInfo(i);
		const FName PackedTextureName = TextureSetsHelpers::MakeTextureParameterName(Args.ParameterName, i);

		TObjectPtr<UMaterialExpressionTextureObjectParameter> TextureObject = CreateExpression<UMaterialExpressionTextureObjectParameter>();
		UTexture* DefaultTexture = Args.DefaultDerivedData->Textures[i].Texture;
		TextureObject->SamplerType = UMaterialExpressionTextureBase::GetSamplerTypeForTexture(DefaultTexture);
		TextureObject->SamplerSource = ESamplerSourceMode::SSM_Wrap_WorldGroupSettings; // So we don't allocate a sampler

		TextureObject->SetParameterName(PackedTextureName);
		FMaterialParameterMetadata Meta;
		Meta.Value.Type = EMaterialParameterType::Texture;
		Meta.Value.Texture = DefaultTexture;
		Meta.Group = Args.ParameterGroup;
		Meta.bHidden = true;
		TextureObject->SetParameterValue(PackedTextureName, Meta, EMaterialExpressionSetParameterValueFlags::AssignGroupAndSortPriority);
		TextureObject->UpdateParameterGuid(true, true);
		EncodedTextureObjects.Add(TextureObject);
	}

	// Created on demand
	EncodedTextureSizes.Init(nullptr, Args.PackingInfo.NumPackedTextures());

	// Call out to modules to do the work of connecting decoded texture samples to outputs
	for (const UTextureSetModule* Module : Args.ModuleInfo.GetModules())
	{
		WorkingModule = Module;
		Module->ConfigureSamplingGraphBuilder(&Args.SampleParams, this);
	}
	WorkingModule = nullptr;

	// Build all the sub-samples
	TMap<FName, FGraphBuilderOutputPin> SampleResults = BuildSubsamplesRecursive(FSubSampleAddress::Root());

	// Create outputs for each of the blended subsample results
	for (const auto& [Name, Address] : SampleResults)
	{
		UMaterialExpressionFunctionOutput* Output = CreateOutput(Name);
		Connect(Address, Output, 0);
	}

	// Resolve all shared value's sources to the reroute nodes
	for (auto& [Address, AddressValues] : SharedValues)
	{
		for (auto& [Type, Value] : AddressValues)
		{
			if (Value.IsUsed())
			{
				check(Value.Source.IsValid());
				check(Value.Reroute.IsValid());
				Connect(Value.Source, Value.Reroute.GetExpression(), 0);
			}
		}
	}
}

const TArray<SubSampleHandle>& FTextureSetSampleFunctionBuilder::AddSubsampleGroup(TArray<FSubSampleDefinition> SampleGroup)
{
	TArray<SubSampleHandle>& SampleGroupHandles = SampleGroups.AddDefaulted_GetRef();

	for (const FSubSampleDefinition& SampleDef : SampleGroup)
	{
		const SubSampleHandle& Handle = SampleGroupHandles.Add_GetRef(SubSampleHandle::NewGuid());
		SubsampleDefinitions.Add(Handle, SampleDef);
	}

	return SampleGroupHandles;
}

UMaterialExpression* FTextureSetSampleFunctionBuilder::CreateFunctionCall(FSoftObjectPath FunctionPath)
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

UMaterialExpressionNamedRerouteDeclaration* FTextureSetSampleFunctionBuilder::CreateReroute(FName Name, const FSubSampleAddress& SampleAddress)
{
	UMaterialExpressionNamedRerouteDeclaration* Reroute = CreateExpression<UMaterialExpressionNamedRerouteDeclaration>();
	Reroute->Name = FName(FString::Format(TEXT("{0}.{1}"), { SubsampleAddressToString(SampleAddress), Name.ToString() }));
	return Reroute;
}

const FGraphBuilderOutputPin& FTextureSetSampleFunctionBuilder::GetSharedValue(const FSubSampleAddress& Address, FName Name)
{
	FGraphBuilderValue& Value = SharedValues.FindOrAdd(Address).FindOrAdd(Name);

	if (!Value.Reroute.IsValid())
	{
		// First time shared value was requested, create a reroute node for it and use that as the output
		Value.Reroute = FGraphBuilderOutputPin(CreateReroute(Name, Address), 0);
	}

	return Value.Reroute;
}

const void FTextureSetSampleFunctionBuilder::SetSharedValue(const FSubSampleAddress& Address, FName Name, FGraphBuilderOutputPin Pin)
{
	FGraphBuilderValue& Value = SharedValues.FindOrAdd(Address).FindOrAdd(Name);

	if (!Value.Source.IsValid())
	{
		Value.Source = Pin;
		Value.Owner = WorkingModule;
	}
	else // Error logging
	{
		if (IsValid(Value.Owner))
		{
			LogError(FText::Format(INVTEXT("Shared value '{0}' at '{1}' has already been set by '{2}'. Multiple overrides are not currently supported"),
				FText::FromName(Name),
				FText::FromString(SubsampleAddressToString(Address)),
				FText::FromString(Value.Owner->GetInstanceName())));
		}
		else
		{
			LogError(FText::Format(INVTEXT("Shared value '{0}' at '{1}' has already been set. Multiple overrides are not currently supported"),
				FText::FromName(Name),
				FText::FromString(SubsampleAddressToString(Address))));
		}
	}
}

const FGraphBuilderOutputPin& FTextureSetSampleFunctionBuilder::GetSharedValue(const FSubSampleAddress& Address, EGraphBuilderSharedValueType ValueType)
{
	return GetSharedValue(Address, UEnum::GetValueAsName(ValueType));
}

void FTextureSetSampleFunctionBuilder::SetSharedValue( const FSubSampleAddress& Address, EGraphBuilderSharedValueType ValueType, FGraphBuilderOutputPin Pin)
{
	SetSharedValue(Address, UEnum::GetValueAsName(ValueType), Pin);
}

void FTextureSetSampleFunctionBuilder::Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, UMaterialExpression* InputNode, uint32 InputIndex)
{
	Connect(FGraphBuilderOutputPin(OutputNode, OutputIndex), FGraphBuilderInputPin(InputNode, InputIndex));
}

void FTextureSetSampleFunctionBuilder::Connect(const FGraphBuilderOutputPin& Output, UMaterialExpression* InputNode, uint32 InputIndex)
{
	Connect(Output, FGraphBuilderInputPin(InputNode, InputIndex));
}

void FTextureSetSampleFunctionBuilder::Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, const FGraphBuilderInputPin& Input)
{
	Connect(FGraphBuilderOutputPin(OutputNode, OutputIndex), Input);
}

void FTextureSetSampleFunctionBuilder::Connect(UMaterialExpression* OutputNode, FName OutputName, UMaterialExpression* InputNode, FName InputName)
{
	Connect(OutputNode, FindOutputIndexChecked(OutputNode, OutputName), InputNode, FindInputIndexChecked(InputNode, InputName));
}

void FTextureSetSampleFunctionBuilder::Connect(const FGraphBuilderOutputPin& Output, UMaterialExpression* InputNode, FName InputName)
{
	Connect(Output, InputNode, FindInputIndexChecked(InputNode, InputName));
}

void FTextureSetSampleFunctionBuilder::Connect(UMaterialExpression* OutputNode, FName OutputName, const FGraphBuilderInputPin& Input)
{
	Connect(OutputNode, FindOutputIndexChecked(OutputNode, OutputName), Input);
}

void FTextureSetSampleFunctionBuilder::Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, UMaterialExpression* InputNode, FName InputName)
{
	Connect(OutputNode, OutputIndex, InputNode, FindInputIndexChecked(InputNode, InputName));
}

void FTextureSetSampleFunctionBuilder::Connect(UMaterialExpression* OutputNode, FName OutputName, UMaterialExpression* InputNode, uint32 InputIndex)
{
	Connect(OutputNode, FindOutputIndexChecked(OutputNode, OutputName), InputNode, InputIndex);
}

void FTextureSetSampleFunctionBuilder::Connect(const FGraphBuilderOutputPin& Output, const FGraphBuilderInputPin& Input)
{
	check(Input.IsValid());
	check(Output.IsValid());
	// Check that there's not already a connection there that we're acidentally overriding
	check(!Output.GetExpression()->IsExpressionConnected(Input.GetExpression()->GetInput(Input.GetIndex()), Output.GetIndex()));

	Output.GetExpression()->ConnectExpression(Input.GetExpression()->GetInput(Input.GetIndex()), Output.GetIndex());
}

FGraphBuilderOutputPin FTextureSetSampleFunctionBuilder::GetEncodedTextureObject(int Index, FGraphBuilderOutputPin StreamingCoord)
{
	UMaterialExpressionTextureObjectParameter* TextureObjectExpression = EncodedTextureObjects[Index];

	// Mat compiler will think there's an extra VT stack if appending streaming def for VT
	// Since it serves no purpose in the VT case, might as well avoid it and avoid confusion
	const FTextureSetPackedTextureDef& TextureDef = Args.PackingInfo.GetPackedTextureDef(Index);
	const bool bVirtualTextureStreaming = TextureDef.bVirtualTextureStreaming;
	if (bVirtualTextureStreaming)
	{
		return FGraphBuilderOutputPin(TextureObjectExpression, 0);
	}

	if (Args.PackingInfo.GetPackedTextureInfo(Index).Flags & (uint8)ETextureSetTextureFlags::Array)
	{
		// Just do a dummy value for the streaming since it thinks it needs a float3
		UMaterialExpression* AppendNode = CreateExpression<UMaterialExpressionAppendVector>();
		Connect(StreamingCoord, AppendNode, 0);
		UMaterialExpressionConstant* ConstantNode = CreateExpression<UMaterialExpressionConstant>();
		Connect(ConstantNode, 0, AppendNode, 1);
		StreamingCoord = FGraphBuilderOutputPin(AppendNode, 0);
	}

	UMaterialExpressionTextureStreamingDef* TextureStreamingDef = CreateExpression<UMaterialExpressionTextureStreamingDef>();
	TextureStreamingDef->SamplerType = TextureObjectExpression->SamplerType;
	TextureStreamingDef->SamplerSource = ESamplerSourceMode::SSM_Wrap_WorldGroupSettings;// So we don't allocate a sampler
	Connect(StreamingCoord, TextureStreamingDef, 0);
	Connect(TextureObjectExpression, 0, TextureStreamingDef, 1);

	return FGraphBuilderOutputPin(TextureStreamingDef, 0);
}

const FGraphBuilderOutputPin FTextureSetSampleFunctionBuilder::GetEncodedTextureSize(int Index)
{
	if (EncodedTextureSizes[Index] == nullptr)
	{
		UMaterialExpressionTextureProperty* TextureProp = CreateExpression<UMaterialExpressionTextureProperty>();
		EncodedTextureObjects[Index]->ConnectExpression(TextureProp->GetInput(0), 0);
		TextureProp->Property = EMaterialExposedTextureProperty::TMTM_TextureSize;
		EncodedTextureSizes[Index] = TextureProp;
	}

	return FGraphBuilderOutputPin(EncodedTextureSizes[Index], 0);
}

const TTuple<FGraphBuilderOutputPin, FGraphBuilderOutputPin> FTextureSetSampleFunctionBuilder::GetRangeCompressParams(int Index)
{
	const FTextureSetPackedTextureInfo& TextureInfo = Args.PackingInfo.GetPackedTextureInfo(Index);

	if (TextureInfo.ChannelEncodings & (uint8)ETextureSetChannelEncoding::RangeCompression)
	{
		if (!ConstantParameters.Contains(TextureInfo.RangeCompressMulName))
			MakeConstantParameter(TextureInfo.RangeCompressAddName, FVector4f::Zero());

		if (!ConstantParameters.Contains(TextureInfo.RangeCompressMulName))
			MakeConstantParameter(TextureInfo.RangeCompressMulName, FVector4f::One());

		return TTuple<FGraphBuilderOutputPin, FGraphBuilderOutputPin>(
			FGraphBuilderOutputPin(ConstantParameters.FindChecked(TextureInfo.RangeCompressMulName), 0),
			FGraphBuilderOutputPin(ConstantParameters.FindChecked(TextureInfo.RangeCompressAddName), 0));
	}
	else
	{
		UMaterialExpressionConstant4Vector* One = CreateExpression<UMaterialExpressionConstant4Vector>();
		One->Constant = FLinearColor::White;
		UMaterialExpressionConstant4Vector* Zero = CreateExpression<UMaterialExpressionConstant4Vector>();
		Zero->Constant = FLinearColor::Black;

		return TTuple<FGraphBuilderOutputPin, FGraphBuilderOutputPin>(
			FGraphBuilderOutputPin(One, 0),
			FGraphBuilderOutputPin(Zero, 0));
	}
}

void FTextureSetSampleFunctionBuilder::LogError(FText ErrorText)
{
	Errors.Add(WorkingModule ? FText::Format(INVTEXT("{0}: {1}"), FText::FromString(WorkingModule->GetInstanceName()), ErrorText) : ErrorText);
}

void FTextureSetSampleFunctionBuilder::AddSubsampleFunction(ConfigureSubsampleFunction Function)
{
	SubsampleFunctions.Add({Function, WorkingModule});
}

UMaterialExpressionFunctionInput* FTextureSetSampleFunctionBuilder::CreateInput(FName Name, EFunctionInputType Type, EInputSort Sort)
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

UMaterialExpressionFunctionOutput* FTextureSetSampleFunctionBuilder::CreateOutput(FName Name)
{
	if (GraphOutputs.Contains(Name))
		LogError(FText::Format(INVTEXT("Output {0} already exists; an input can only be created once."), FText::FromName(Name)));

	UMaterialExpressionFunctionOutput* NewOutput = CreateExpression<UMaterialExpressionFunctionOutput>();
	NewOutput->OutputName = Name;
	GraphOutputs.Add(Name, NewOutput);
	return NewOutput;
}

UMaterialExpression* FTextureSetSampleFunctionBuilder::MakeConstantParameter(FName Name, FVector4f Default)
{
	UMaterialExpressionVectorParameter* NewParam = CreateExpression<UMaterialExpressionVectorParameter>();
	NewParam->ParameterName = TextureSetsHelpers::MakeConstantParameterName(Args.ParameterName, Name);
	NewParam->DefaultValue = FLinearColor(Default);
	NewParam->Group = Args.ParameterGroup;
	NewParam->bHidden = true;

	// Main pin on the parameter is actually a float3, need to append the alpha to make it a float4.
	UMaterialExpression* AppendNode = CreateExpression<UMaterialExpressionAppendVector>();
	NewParam->ConnectExpression(AppendNode->GetInput(0), 0);
	NewParam->ConnectExpression(AppendNode->GetInput(1), 4);

	ConstantParameters.Add(Name, AppendNode);
	return AppendNode;
}

TMap<FName, FGraphBuilderOutputPin> FTextureSetSampleFunctionBuilder::BuildSubsamplesRecursive(const FSubSampleAddress& Address)
{
	const int32 Depth = Address.GetHandleChain().Num();
	check(Depth <= SampleGroups.Num());
	const bool IsLeaf = Depth == SampleGroups.Num();

	if (IsLeaf)
	{
		FTextureSetSubsampleBuilder Context(this, Address);

		for (auto& [SampleBuilder, Module] : SubsampleFunctions)
		{
			WorkingModule = Module;
			SampleBuilder(Context);
		}
		WorkingModule = nullptr;

		SetupTextureValues(Context);
		SetupFallbackValues(Address);

		return Context.Results;
	}
	else
	{
		const TArray<SubSampleHandle>& SampleGroup = SampleGroups[Depth];

		TArray<TMap<FName, FGraphBuilderOutputPin>> Results;

		for (SubSampleHandle SampleHandle : SampleGroup)
		{
			Results.Add(BuildSubsamplesRecursive(FSubSampleAddress(Address, SampleHandle)));
		}

		SetupFallbackValues(Address);

		return BlendSubsampleResults(Address, Results);
	}
}

TMap<FName, FGraphBuilderOutputPin> FTextureSetSampleFunctionBuilder::BlendSubsampleResults(const FSubSampleAddress& Address, TArray<TMap<FName, FGraphBuilderOutputPin>> Results)
{
	TMap<FName, FGraphBuilderOutputPin> BlendedResult;

	// Validate all results share the same elements
	for (const auto& [Name, Output] : Results[0])
	{
		for (int i = 1; i < Results.Num(); i++)
			check(Results[i].Contains(Name));
	}

	// For each set of results
	for (int i = 0; i < Results.Num(); i++)
	{
		const FGraphBuilderOutputPin& SampleWeight = SubsampleDefinitions.FindChecked(SampleGroups[Address.GetHandleChain().Num()][i]).Weight;

		for (const auto& [Name, Result] : Results[i])
		{
			// Multiply each result by the sample weight
			UMaterialExpressionMultiply* Mul = CreateExpression<UMaterialExpressionMultiply>();
			Connect(Result, Mul, 0);
			Connect(SampleWeight, Mul, 1);

			if (FGraphBuilderOutputPin* PrevBlended = BlendedResult.Find(Name))
			{
				// Add the weighted results together
				UMaterialExpressionAdd* Add = CreateExpression<UMaterialExpressionAdd>();
				Connect(*PrevBlended, Add, 0);
				Connect(FGraphBuilderOutputPin(Mul, 0), Add, 1);
				BlendedResult.Add(Name, FGraphBuilderOutputPin(Add, 0));
			}
			else
			{
				BlendedResult.Add(Name, FGraphBuilderOutputPin(Mul, 0));
			}
		}
	}

	return BlendedResult;
}

void FTextureSetSampleFunctionBuilder::SetupFallbackValues(const FSubSampleAddress& Address)
{
	// Sets up fallback values for shared values that were requested, but were not explicity provided.

	const auto NeedsValue = [this, &Address](EGraphBuilderSharedValueType ValueType)
	{
		if (!SharedValues.Contains(Address))
			return false;

		FGraphBuilderValue* Value = SharedValues[Address].Find(UEnum::GetValueAsName(ValueType));

		if (Value == nullptr)
			return false;

		return Value->IsUsed() && !Value->Source.IsValid();
	};

	// Handle cases where we fall back to a local shared value, instead of the parent's shared value
	// Note: Order here is important, as we have some dependencies
	if (NeedsValue(EGraphBuilderSharedValueType::Texcoord_Streaming))
	{
		SetSharedValue(Address, EGraphBuilderSharedValueType::Texcoord_Streaming, GetSharedValue(Address, EGraphBuilderSharedValueType::Texcoord_Raw));
	}

	if (NeedsValue(EGraphBuilderSharedValueType::Texcoord_DDX))
	{
		UMaterialExpression* DDX = CreateExpression<UMaterialExpressionDDX>();
		Connect(GetSharedValue(Address, EGraphBuilderSharedValueType::Texcoord_Mip), FGraphBuilderInputPin(DDX, 0));
		SetSharedValue(Address, EGraphBuilderSharedValueType::Texcoord_DDX, FGraphBuilderOutputPin(DDX, 0));
	}

	if (NeedsValue(EGraphBuilderSharedValueType::Texcoord_DDY))
	{
		UMaterialExpression* DDY = CreateExpression<UMaterialExpressionDDY>();
		Connect(GetSharedValue(Address, EGraphBuilderSharedValueType::Texcoord_Mip), FGraphBuilderInputPin(DDY, 0));
		SetSharedValue(Address, EGraphBuilderSharedValueType::Texcoord_DDY, FGraphBuilderOutputPin(DDY, 0));
	}

	if (NeedsValue(EGraphBuilderSharedValueType::Texcoord_Mip))
	{
		SetSharedValue(Address, EGraphBuilderSharedValueType::Texcoord_Mip, GetSharedValue(Address, EGraphBuilderSharedValueType::Texcoord_Raw));
	}

	if (NeedsValue(EGraphBuilderSharedValueType::Texcoord_Sampling))
	{
		SetSharedValue(Address, EGraphBuilderSharedValueType::Texcoord_Sampling, GetSharedValue(Address, EGraphBuilderSharedValueType::Texcoord_Raw));
	}

	if (!Address.IsRoot())
	{
		// For all non-root subsamples, fall back to the shared value in the parent subsample
		for (int i = 0; i < (int)EGraphBuilderSharedValueType::Num; i++)
		{
			EGraphBuilderSharedValueType t = (EGraphBuilderSharedValueType)i;

			if (NeedsValue(t))
			{
				SetSharedValue(Address, t, GetSharedValue(Address.GetParent(), t));
			}
		}
	}
	else
	{
		// For the root values, fallback based on node's context
		// Note: Order here is important, as we have some dependencies
		const bool NeedsTangent = NeedsValue(EGraphBuilderSharedValueType::Tangent);
		const bool NeedsBitangent = NeedsValue(EGraphBuilderSharedValueType::Bitangent);

		if (NeedsTangent || NeedsBitangent)
		{
			FGraphBuilderOutputPin TangentSource;
			FGraphBuilderOutputPin BitangentSource;
			switch (Args.SampleContext.TangentSource)
			{
			case ETangentSource::Explicit:
			{
				TangentSource = FGraphBuilderOutputPin(CreateInput("Tangent", EFunctionInputType::FunctionInput_Vector3, EInputSort::Tangent), 0);
				BitangentSource = FGraphBuilderOutputPin(CreateInput("Bitangent", EFunctionInputType::FunctionInput_Vector3, EInputSort::Tangent), 0);
				break;
			}
			case ETangentSource::Synthesized:
			{
				UMaterialExpression* SynthesizeTangentNode = MakeSynthesizeTangentCustomNode(
					GetSharedValue(Address, EGraphBuilderSharedValueType::Position),
					GetSharedValue(Address, EGraphBuilderSharedValueType::Texcoord_Raw),
					GetSharedValue(Address, EGraphBuilderSharedValueType::BaseNormal));

				TangentSource = FGraphBuilderOutputPin(SynthesizeTangentNode, 1);
				BitangentSource = FGraphBuilderOutputPin(SynthesizeTangentNode, 2);
				break;
			}
			case ETangentSource::Vertex:
			{
				TangentSource = FGraphBuilderOutputPin(CreateExpression<UMaterialExpressionVertexTangentWS>(), 0);

				if (NeedsBitangent)
				{
					// Don't have a node explicitly for the vertex bitangent, so we create it by transforming a vector from tangent to world space
					UMaterialExpressionConstant3Vector* BitangentConstant = CreateExpression<UMaterialExpressionConstant3Vector>();
					BitangentConstant->Constant = FLinearColor(0, 1, 0);
					UMaterialExpressionTransform* TransformBitangent = CreateExpression<UMaterialExpressionTransform>();
					TransformBitangent->TransformSourceType = EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_Tangent;
					TransformBitangent->TransformType = EMaterialVectorCoordTransform::TRANSFORM_World;
					Connect(BitangentConstant, 0, TransformBitangent, 0);
					BitangentSource = FGraphBuilderOutputPin(TransformBitangent, 0);
				}
				break;
			}
			default:
				unimplemented();
				break;
			}

			if (NeedsTangent)
				SetSharedValue(Address, EGraphBuilderSharedValueType::Tangent, TangentSource);

			if (NeedsBitangent)
				SetSharedValue(Address, EGraphBuilderSharedValueType::Bitangent, BitangentSource);
		}

		if (NeedsValue(EGraphBuilderSharedValueType::BaseNormal))
		{
			FGraphBuilderOutputPin BaseNormalSource;

			switch (Args.SampleContext.BaseNormalSource)
			{
			case EBaseNormalSource::Explicit:
				BaseNormalSource = FGraphBuilderOutputPin(CreateInput("Base Normal", EFunctionInputType::FunctionInput_Vector3, EInputSort::BaseNormal), 0);
				break;
			case EBaseNormalSource::Vertex:
				BaseNormalSource = FGraphBuilderOutputPin(CreateExpression<UMaterialExpressionVertexNormalWS>(), 0);
				break;
			default:
				unimplemented();
				break;
			}

			SetSharedValue(Address, EGraphBuilderSharedValueType::BaseNormal, BaseNormalSource);
		}

		if (NeedsValue(EGraphBuilderSharedValueType::CameraVector))
		{
			FGraphBuilderOutputPin CameraVectorSource;

			switch (Args.SampleContext.CameraVectorSource)
			{
			case ECameraVectorSource::Explicit:
				CameraVectorSource = FGraphBuilderOutputPin(CreateInput("Camera Vector", EFunctionInputType::FunctionInput_Vector3, EInputSort::CameraVector), 0);
				break;
			case ECameraVectorSource::World:
				CameraVectorSource = FGraphBuilderOutputPin(CreateExpression<UMaterialExpressionCameraVectorWS>(), 0);
				break;
			default:
				unimplemented();
				break;
			}

			SetSharedValue(Address, EGraphBuilderSharedValueType::CameraVector, CameraVectorSource);
		}

		if (NeedsValue(EGraphBuilderSharedValueType::Position))
		{
			FGraphBuilderOutputPin PositionSource;

			switch (Args.SampleContext.PositionSource)
			{
			case EPositionSource::Explicit:
				PositionSource = FGraphBuilderOutputPin(CreateInput("Position", EFunctionInputType::FunctionInput_Vector3, EInputSort::Position), 0);
				break;
			case EPositionSource::World:
				PositionSource = FGraphBuilderOutputPin(CreateExpression<UMaterialExpressionWorldPosition>(), 0);
				break;
			default:
				unimplemented();
				break;
			}

			SetSharedValue(Address, EGraphBuilderSharedValueType::Position, PositionSource);
		}

		if (NeedsValue(EGraphBuilderSharedValueType::Texcoord_Raw))
		{
			UMaterialExpression* UVInput = CreateInput("UV", EFunctionInputType::FunctionInput_Vector2, EInputSort::UV);
			SetSharedValue(Address, EGraphBuilderSharedValueType::Texcoord_Raw, FGraphBuilderOutputPin(UVInput, 0));
		}

		if (NeedsValue(EGraphBuilderSharedValueType::ArrayIndex))
		{
			FGraphBuilderOutputPin IndexInput(CreateInput("Array Index", EFunctionInputType::FunctionInput_Scalar, EInputSort::UV), 0);
			SetSharedValue(Address, EGraphBuilderSharedValueType::ArrayIndex, IndexInput);
		}
	}
}

void FTextureSetSampleFunctionBuilder::SetupTextureValues(FTextureSetSubsampleBuilder& Subsample)
{
	// Sample each encoded/packed texture, and create a shared value for each channel.
	for (int t = 0; t < Args.PackingInfo.NumPackedTextures(); t++)
	{
		UMaterialExpression* DecodedTexture = BuildTextureDecodeNode(t, Subsample);

		const TArray<FName> SourceChannels = Args.PackingInfo.GetPackedTextureDef(t).GetSources();
		for (int c = 0; c < SourceChannels.Num(); c++)
		{
			if (SourceChannels[c].IsNone())
				continue;

			Subsample.SetSharedValue(SourceChannels[c], FGraphBuilderOutputPin(DecodedTexture, c + 1));
		}
	}

	// Append vectors back into their equivalent processed textures
	// This undoes the texture packing
	for (const auto& [TextureName, ProcessedTexture] : Args.ModuleInfo.GetProcessedTextures())
	{
		FGraphBuilderOutputPin WorkingPin = Subsample.GetSharedValue(FName(TextureName.ToString() + TextureSetsHelpers::ChannelSuffixes[0]));

		for (int i = 1; i < ProcessedTexture.ChannelCount; i++)
		{
			UMaterialExpression* AppendNode = CreateExpression<UMaterialExpressionAppendVector>();
			FGraphBuilderOutputPin NextChannel = GetSharedValue(Subsample.Address, FName(TextureName.ToString() + TextureSetsHelpers::ChannelSuffixes[i]));
			Connect(WorkingPin, AppendNode, 0);
			Connect(NextChannel, AppendNode, 1);
			WorkingPin = FGraphBuilderOutputPin(AppendNode, 0);
		}

		Subsample.SetSharedValue(TextureName, WorkingPin);
	}
}

UMaterialExpression* FTextureSetSampleFunctionBuilder::BuildTextureDecodeNode(int Index, FTextureSetSubsampleBuilder& Context)
{
	const FTextureSetPackedTextureDef& TextureDef = Args.PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo& TextureInfo = Args.PackingInfo.GetPackedTextureInfo(Index);

	FGraphBuilderOutputPin TextureObject = GetEncodedTextureObject(
		Index,
		Context.GetSharedValue(EGraphBuilderSharedValueType::Texcoord_Streaming));

	FGraphBuilderOutputPin SampleCoord = Context.GetSharedValue(EGraphBuilderSharedValueType::Texcoord_Sampling);
	FGraphBuilderOutputPin DDX = Context.GetSharedValue(EGraphBuilderSharedValueType::Texcoord_DDX);
	FGraphBuilderOutputPin DDY = Context.GetSharedValue(EGraphBuilderSharedValueType::Texcoord_DDY);

	// Append the array index to out UV to get the sample coordinate
	if (TextureInfo.Flags & (uint8)ETextureSetTextureFlags::Array)
	{
		UMaterialExpression* AppendNode = CreateExpression<UMaterialExpressionAppendVector>();
		Connect(SampleCoord, AppendNode, 0);
		Connect(Context.GetSharedValue(EGraphBuilderSharedValueType::ArrayIndex), AppendNode, 1);
		SampleCoord = FGraphBuilderOutputPin(AppendNode, 0);
	}

	static const FString ChannelSuffixLower[4] {"r", "g", "b", "a"};
	static const FString ChannelSuffixUpper[4] {"R", "G", "B", "A"};

	const bool bVirtualTextureStreaming = TextureDef.bVirtualTextureStreaming;

	UMaterialExpressionCustom* CustomExp = CreateExpression<UMaterialExpressionCustom>();
	CustomExp->SamplerSourceMode = SSM_Wrap_WorldGroupSettings;
	CustomExp->Inputs.Empty(); // required: class initializes with one input by default
	CustomExp->IncludeFilePaths.Add("/Engine/Private/Common.ush");
	CustomExp->Code = "";

	FString SampleType = FString::Format(TEXT("MaterialFloat{0} Sample = "), {(TextureInfo.ChannelCount > 1) ? FString::FromInt(TextureInfo.ChannelCount) : ""});

	if (bVirtualTextureStreaming)
	{
		TObjectPtr<UMaterialExpressionTextureSample> SampleExpression = CreateExpression<UMaterialExpressionTextureSample>();
		// We know this should be a UMaterialExpressionTextureObjectParameter when using VTs
		TObjectPtr<const UMaterialExpressionTextureObjectParameter> TextureObjExpression = CastChecked<UMaterialExpressionTextureObjectParameter>(TextureObject.GetExpression());
		SampleExpression->SamplerType = TextureObjExpression->SamplerType;
		SampleExpression->SamplerSource = ESamplerSourceMode::SSM_Wrap_WorldGroupSettings;
		SampleExpression->MipValueMode = TMVM_Derivative;

		Connect(TextureObject, SampleExpression, "TextureObject");
		Connect(DDX, SampleExpression, "DDX(UVs)");
		Connect(DDY, SampleExpression, "DDY(UVs)");
		Connect(SampleCoord, SampleExpression, "Coordinates");

		CustomExp->Inputs.Add({"InSample"});

		constexpr uint32 RGBAOutputIndex = 5;
		Connect(FGraphBuilderOutputPin(SampleExpression, RGBAOutputIndex), CustomExp, 0);

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

	CustomExp->Description = FString::Format(TEXT("{0}_Decode"), {TextureSetsHelpers::MakeTextureParameterName(Args.ParameterName, Index).ToString()});

	return CustomExp;
}

UMaterialExpression* FTextureSetSampleFunctionBuilder::MakeSynthesizeTangentCustomNode(const FGraphBuilderOutputPin& Position, const FGraphBuilderOutputPin& Texcoord, const FGraphBuilderOutputPin& Normal)
{
	HLSLFunctionCallNodeBuilder FunctionCall("SynthesizeTangents", "/Plugin/TextureSets/NormalSynthesis.ush");

	FunctionCall.InArgument("Position", Position);
	FunctionCall.InArgument("Texcoord", Texcoord);
	FunctionCall.InArgument("Normal", Normal);

	FunctionCall.OutArgument("Tangent", ECustomMaterialOutputType::CMOT_Float3);
	FunctionCall.OutArgument("Bitangent", ECustomMaterialOutputType::CMOT_Float3);

	return FunctionCall.Build(this);
}

int32 FTextureSetSampleFunctionBuilder::FindInputIndexChecked(UMaterialExpression* InputNode, FName InputName)
{
	int32 InputIndex = -1;
	FString FoundNodes;
	for (FExpressionInputIterator It{ InputNode }; It; ++It)
	{
		FName InputNodeName = InputNode->GetInputName(It.Index);
		if (InputNodeName.Compare(InputName) == 0)
		{
			InputIndex = It.Index;
			break;
		}
		else
		{
			FoundNodes.Append(InputNodeName.ToString() + ", ");
		}
	}
	checkf(InputIndex >= 0, TEXT("Could not find input with name %s\r\nFound nodes: %s"), *InputName.ToString(), *FoundNodes);
	return InputIndex;
}

int32 FTextureSetSampleFunctionBuilder::FindOutputIndexChecked(UMaterialExpression* OutputNode, FName OutputName)
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
	checkf(OutputIndex >= 0, TEXT("Could not find output with name %s"), *OutputName.ToString());
	return OutputIndex;
}

FString FTextureSetSampleFunctionBuilder::SubsampleAddressToString(const FSubSampleAddress& Address)
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