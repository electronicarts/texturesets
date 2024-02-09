// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "GraphBuilderGraphAddress.h"
#include "GraphBuilderValue.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "TextureSetAssetParams.h"
#include "TextureSetDerivedData.h"
#include "TextureSetInfo.h"
#include "TextureSetSubsampleContext.h"
#include "TextureSetSampleContext.h"
#endif

class UTextureSetDefinition;
class UMaterialExpressionTextureSetSampleParameter;
class UMaterialExpressionFunctionOutput;
class UMaterialExpressionTextureObjectParameter;
class UMaterialExpressionNamedRerouteDeclaration;
class UMaterialExpressionMaterialFunctionCall;


#if WITH_EDITOR
struct FTextureSetMaterialGraphBuilderArgs
{
	FName ParameterName;
	FName ParameterGroup;
	UMaterialFunction* MaterialFunction;
	FTextureSetDefinitionModuleInfo ModuleInfo;
	FTextureSetPackingInfo PackingInfo;
	const FTextureSetDerivedData* DefaultDerivedData;
	FTextureSetAssetParamsCollection SampleParams;
	FTextureSetSampleContext SampleContext;
};

// Class responsible for building the material graph of a texture set sampler node.
// Texture set modules use this node to customize the sampling logic.
class TEXTURESETSMATERIALBUILDER_API FTextureSetMaterialGraphBuilder
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

	FTextureSetMaterialGraphBuilder(const FTextureSetMaterialGraphBuilderArgs& Args);

	// Change this to regenerate all texture set material graphs
	static FString GetGraphBuilderVersion() { return "0.3"; }

	template <class T> T* CreateExpression()
	{
		return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(Args.MaterialFunction, T::StaticClass()));
	}

	UMaterialExpression* CreateFunctionCall(FSoftObjectPath FunctionPath);
	UMaterialExpressionFunctionInput* CreateInput(FName Name, EFunctionInputType Type, EInputSort Sort = EInputSort::Custom);
	UMaterialExpressionFunctionOutput* CreateOutput(FName Name);
	UMaterialExpression* MakeConstantParameter(FName Name, FVector4f Default);

	UMaterialExpressionNamedRerouteDeclaration* CreateReroute(FName Name);
	UMaterialExpressionNamedRerouteDeclaration* CreateReroute(const FString& Name, const FString& Namespace);

	const TArray<SubSampleHandle>& AddSubsampleGroup(TArray<FSubSampleDefinition> SampleGroup);
	void AddSampleBuilder(SampleBuilderFunction SampleBuilder);

	// Whole bunch of overloads for connecting stuff, hopefully makes it easy
	void Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, UMaterialExpression* InputNode, uint32 InputIndex);
	void Connect(const FGraphBuilderOutputAddress& Output, UMaterialExpression* InputNode, uint32 InputIndex);
	void Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, const FGraphBuilderInputAddress& Input);
	void Connect(UMaterialExpression* OutputNode, FName OutputName, UMaterialExpression* InputNode, FName InputName);
	void Connect(const FGraphBuilderOutputAddress& Output, UMaterialExpression* InputNode, FName InputName);
	void Connect(UMaterialExpression* OutputNode, FName OutputName, const FGraphBuilderInputAddress& Input);
	void Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, UMaterialExpression* InputNode, FName InputName);
	void Connect(UMaterialExpression* OutputNode, FName OutputName, UMaterialExpression* InputNode, uint32 InputIndex);
	void Connect(const FGraphBuilderOutputAddress& Output, const FGraphBuilderInputAddress& Input);
	
	FGraphBuilderOutputAddress GetPackedTextureObject(int Index, FGraphBuilderOutputAddress StreamingCoord);
	const FGraphBuilderOutputAddress GetPackedTextureSize(int Index);
	const TTuple<int, int> GetPackingSource(FName ProcessedTextureChannel) { return Args.PackingInfo.GetPackingSource(ProcessedTextureChannel); }
	// Gets the addresses of the range compress multiply and add parameters for a specific packed texture.
	const TTuple<FGraphBuilderOutputAddress, FGraphBuilderOutputAddress> GetRangeCompressParams(int Index);

	void LogError(FText ErrorText);
	const TArray<FText>& GetErrors() const { return Errors; }

	FString SubsampleAddressToString(const FSubSampleAddress& Address);

	const FGraphBuilderOutputAddress& GetFallbackValue(EGraphBuilderSharedValueType Value);
	const void SetFallbackValue(FGraphBuilderOutputAddress Address, EGraphBuilderSharedValueType Value);

	const UTextureSetModule* GetWorkingModule() { return WorkingModule; }
private:

	const FTextureSetMaterialGraphBuilderArgs Args;

	TArray<TArray<SubSampleHandle>> SampleGroups;
	TMap<SubSampleHandle, FSubSampleDefinition> SubsampleDefinitions;

	TArray<TObjectPtr<UMaterialExpressionTextureObjectParameter>> PackedTextureObjects;
	TArray<TObjectPtr<UMaterialExpression>> PackedTextureSizes;

	TMap<FName, TObjectPtr<UMaterialExpression>> ConstantParameters;

	TMap<FName, TObjectPtr<UMaterialExpressionFunctionInput>> GraphInputs;
	TMap<FName, TObjectPtr<UMaterialExpressionFunctionOutput>> GraphOutputs;

	TMap<EGraphBuilderSharedValueType, FGraphBuilderValue> FallbackValues;

	TMap<FGraphBuilderInputAddress, FGraphBuilderOutputAddress> DeferredConnections;

	TArray<TTuple<SampleBuilderFunction, const class UTextureSetModule*>> SampleBuilders;

	TArray<FText> Errors;

	// The module that is currently executing some code (if any)
	// Should mainly be used for error reporting and validation
	const UTextureSetModule* WorkingModule;

	TMap<FName, FGraphBuilderOutputAddress> BuildSubsamplesRecursive(const FSubSampleAddress& Address);
	TMap<FName, FGraphBuilderOutputAddress> BlendSubsampleResults(const FSubSampleAddress& Address, TArray<TMap<FName, FGraphBuilderOutputAddress>>);

	void SetupFallbackValues();
	void SetupSharedValues(FTextureSetSubsampleContext& Context);
	void SetupTextureValues(FTextureSetSubsampleContext& Context);

	UMaterialExpression* MakeTextureSamplerCustomNode(int Index, FTextureSetSubsampleContext& Context);
	UMaterialExpression* MakeSynthesizeTangentCustomNode(const FGraphBuilderOutputAddress& Position, const FGraphBuilderOutputAddress& Texcoord, const FGraphBuilderOutputAddress& Normal);

	static int32 FindInputIndexChecked(UMaterialExpression* InputNode, FName InputName);
	static int32 FindOutputIndexChecked(UMaterialExpression* OutputNode, FName OutputName);
};
#endif // WITH_EDITOR
