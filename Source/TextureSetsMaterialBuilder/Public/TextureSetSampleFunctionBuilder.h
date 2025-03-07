// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "GraphBuilderPin.h"
#include "GraphBuilderValue.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "TextureSetAssetParams.h"
#include "TextureSetDerivedData.h"
#include "TextureSetInfo.h"
#include "TextureSetSubsampleBuilder.h"
#include "TextureSetSampleContext.h"

class UTextureSetDefinition;
class UMaterialExpressionTextureSetSampleParameter;
class UMaterialExpressionFunctionOutput;
class UMaterialExpressionTextureObjectParameter;
class UMaterialExpressionNamedRerouteDeclaration;
class UMaterialExpressionMaterialFunctionCall;

struct FTextureSetMaterialGraphBuilderArgs
{
	FName ParameterName;
	FName ParameterGroup;
	UMaterialFunction* MaterialFunction;
	FTextureSetDefinitionModuleInfo ModuleInfo;
	FTextureSetPackingInfo PackingInfo;
	const UTextureSetDerivedData* DefaultDerivedData;
	FTextureSetAssetParamsCollection SampleParams;
	FTextureSetSampleContext SampleContext;
};

// Class responsible for building the material graph of a texture set sampler node.
// Texture set modules use this node to customize the sampling logic.
class TEXTURESETSMATERIALBUILDER_API FTextureSetSampleFunctionBuilder
{
public:
	// Used to define SortPriority of input pins, regardless of which order they're created in.
	enum class EInputSort : uint8
	{
		UV,
		BaseNormal,
		Tangent,
		Position,
		CameraVector,
		Custom,
	};

	FTextureSetSampleFunctionBuilder(const FTextureSetMaterialGraphBuilderArgs& Args);

	template <class T> T* CreateExpression()
	{
		return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(Args.MaterialFunction, T::StaticClass()));
	}

	UMaterialExpression* CreateFunctionCall(FSoftObjectPath FunctionPath);
	UMaterialExpressionFunctionInput* CreateInput(FName Name, EFunctionInputType Type, EInputSort Sort = EInputSort::Custom);
	UMaterialExpressionFunctionOutput* CreateOutput(FName Name);
	UMaterialExpression* MakeConstantParameter(FName Name, FVector4f Default);

	UMaterialExpressionNamedRerouteDeclaration* CreateReroute(FName Name, const FSubSampleAddress& SampleAddress);

	const TArray<SubSampleHandle>& AddSubsampleGroup(TArray<FSubSampleDefinition> SampleGroup);
	void AddSubsampleFunction(ConfigureSubsampleFunction SampleBuilder);

	// Whole bunch of overloads for connecting stuff, hopefully makes it easy
	void Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, UMaterialExpression* InputNode, uint32 InputIndex);
	void Connect(const FGraphBuilderOutputPin& Output, UMaterialExpression* InputNode, uint32 InputIndex);
	void Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, const FGraphBuilderInputPin& Input);
	void Connect(UMaterialExpression* OutputNode, FName OutputName, UMaterialExpression* InputNode, FName InputName);
	void Connect(const FGraphBuilderOutputPin& Output, UMaterialExpression* InputNode, FName InputName);
	void Connect(UMaterialExpression* OutputNode, FName OutputName, const FGraphBuilderInputPin& Input);
	void Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, UMaterialExpression* InputNode, FName InputName);
	void Connect(UMaterialExpression* OutputNode, FName OutputName, UMaterialExpression* InputNode, uint32 InputIndex);
	void Connect(const FGraphBuilderOutputPin& Output, const FGraphBuilderInputPin& Input);
	
	FGraphBuilderOutputPin GetEncodedTextureObject(int Index, FGraphBuilderOutputPin StreamingCoord);
	const FGraphBuilderOutputPin GetEncodedTextureSize(int Index);
	const TTuple<int, int> GetPackingSource(FName DecodedTextureChannel) { return Args.PackingInfo.GetPackingSource(DecodedTextureChannel); }
	// Gets the addresses of the range compress multiply and add parameters for a specific encoded texture.
	const TTuple<FGraphBuilderOutputPin, FGraphBuilderOutputPin> GetRangeCompressParams(int Index);

	void LogError(FText ErrorText);
	const TArray<FText>& GetErrors() const { return Errors; }

	FString SubsampleAddressToString(const FSubSampleAddress& Address);

	const FGraphBuilderOutputPin& GetSharedValue(const FSubSampleAddress& Address, FName Name);
	const void SetSharedValue(const FSubSampleAddress& Address, FName Name, FGraphBuilderOutputPin Pin);

	const FGraphBuilderOutputPin& GetSharedValue(const FSubSampleAddress& Address, EGraphBuilderSharedValueType ValueType);
	void SetSharedValue(const FSubSampleAddress& Address, EGraphBuilderSharedValueType ValueType, FGraphBuilderOutputPin Pin);

	const UTextureSetModule* GetWorkingModule() const { return WorkingModule; }
private:

	const FTextureSetMaterialGraphBuilderArgs Args;

	TArray<TArray<SubSampleHandle>> SampleGroups;
	TMap<SubSampleHandle, FSubSampleDefinition> SubsampleDefinitions;

	TArray<TObjectPtr<UMaterialExpressionTextureObjectParameter>> EncodedTextureObjects;
	TArray<TObjectPtr<UMaterialExpression>> EncodedTextureSizes;

	TMap<FName, TObjectPtr<UMaterialExpression>> ConstantParameters;

	TMap<FName, TObjectPtr<UMaterialExpressionFunctionInput>> GraphInputs;
	TMap<FName, TObjectPtr<UMaterialExpressionFunctionOutput>> GraphOutputs;

	TMap<FSubSampleAddress, TMap<FName, FGraphBuilderValue>> SharedValues;

	TArray<TTuple<ConfigureSubsampleFunction, const class UTextureSetModule*>> SubsampleFunctions;

	TArray<FText> Errors;

	// The module that is currently executing some code (if any)
	// Should mainly be used for error reporting and validation
	const UTextureSetModule* WorkingModule;

	TMap<FName, FGraphBuilderOutputPin> BuildSubsamplesRecursive(const FSubSampleAddress& Address);
	TMap<FName, FGraphBuilderOutputPin> BlendSubsampleResults(const FSubSampleAddress& Address, TArray<TMap<FName, FGraphBuilderOutputPin>>);

	void SetupFallbackValues(const FSubSampleAddress& Address);
	void SetupTextureValues(FTextureSetSubsampleBuilder& Context);

	UMaterialExpression* BuildTextureDecodeNode(int Index, FTextureSetSubsampleBuilder& Context);
	UMaterialExpression* MakeSynthesizeTangentCustomNode(const FGraphBuilderOutputPin& Position, const FGraphBuilderOutputPin& Texcoord, const FGraphBuilderOutputPin& Normal);

	static int32 FindInputIndexChecked(UMaterialExpression* InputNode, FName InputName);
	static int32 FindOutputIndexChecked(UMaterialExpression* OutputNode, FName OutputName);
};
