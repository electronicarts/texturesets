// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionCustom.h"
#include "TextureSetInfo.h"

#if WITH_EDITOR

class UTextureSetDefinition;
class UMaterialExpressionTextureSetSampleParameter;
class UMaterialExpressionFunctionOutput;
class UMaterialExpressionTextureObjectParameter;
class UMaterialExpressionNamedRerouteDeclaration;

// Class responsible for building the material graph of a texture set sampler node.
// Texture set modules use this node to customize the sampling logic.
class FTextureSetMaterialGraphBuilder
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

public:

	FTextureSetMaterialGraphBuilder(UMaterialFunction* MaterialFunction, const UMaterialExpressionTextureSetSampleParameter* Node);

	template <class T> T* CreateExpression()
	{
		return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, T::StaticClass()));
	}

	// Change this to regenerate all texture set material graphs
	static FString GetGraphBuilderVersion() { return "1.0"; }

	UMaterialExpression* GetTexcoord();
	void OverrideTexcoord(UMaterialExpression* NewTexcoord, bool OverrideDerivatives, bool OverrideStreaming);
	UMaterialExpression* GetRawTexcoord() const;
	UMaterialExpression* GetTexcoordDerivativeX() const;
	UMaterialExpression* GetTexcoordDerivativeY() const;
	UMaterialExpression* GetTexcoordStreaming() const;

	UMaterialExpression* GetBaseNormal();
	UMaterialExpression* GetTangent();
	UMaterialExpression* GetBitangent();
	UMaterialExpression* GetCameraVector();
	UMaterialExpression* GetPosition();

	UMaterialExpression* GetProcessedTextureSample(FName Name);
	
	UMaterialExpressionTextureObjectParameter* GetPackedTextureObject(int Index);
	UMaterialExpression* GetPackedTextureSize(int Index);

	TTuple<int, int> GetPackingSource(FName ProcessedTextureChannel);

	UMaterialExpressionFunctionInput* CreateInput(FName Name, EFunctionInputType Type, EInputSort Sort = EInputSort::Custom);
	UMaterialExpressionFunctionInput* GetInput(FName Name) const;

	UMaterialExpressionFunctionOutput* CreateOutput(FName Name);
	UMaterialExpressionFunctionOutput* GetOutput(FName Name) const;

	UMaterialExpression* MakeConstantParameter(FName Name, FVector4f Default);

private:
	TObjectPtr<const UMaterialExpressionTextureSetSampleParameter> Node;
	TObjectPtr<const UTextureSetDefinition> Definition;
	TObjectPtr<UMaterialFunction> MaterialFunction;

	const FTextureSetDefinitionModuleInfo ModuleInfo;
	const FTextureSetPackingInfo PackingInfo;

	TArray<TObjectPtr<UMaterialExpressionTextureObjectParameter>> PackedTextureObjects;
	TArray<TObjectPtr<UMaterialExpression>> PackedTextureSamples;
	TArray<TObjectPtr<UMaterialExpression>> PackedTextureSizes;

	// Valid for both texture names "BaseColor" and with channel suffix "BaseColor.g"
	TMap<FName, TObjectPtr<UMaterialExpression>> ProcessedTextureSamples;

	TMap<FName, TObjectPtr<UMaterialExpression>> ConstantParameters;

	// Key is the channel you want to find (e.g. "Roughness.r" or "Normal.g")
	// Value is a tuple of the packed texture and channel where you'll find it
	TMap<FName, TTuple<int, int>> PackingSource;

	TMap<FName, TObjectPtr<UMaterialExpressionFunctionInput>> SampleInputs;
	TMap<FName, TObjectPtr<UMaterialExpressionFunctionOutput>> SampleOutputs;

	TObjectPtr<UMaterialExpressionNamedRerouteDeclaration> TexcoordReroute;
	TObjectPtr<UMaterialExpressionNamedRerouteDeclaration> TexcoordStreamingReroute;
	TObjectPtr<UMaterialExpression> TexcoordRaw;
	TObjectPtr<UMaterialExpression> TexcoordDerivativeX;
	TObjectPtr<UMaterialExpression> TexcoordDerivativeY;

	TObjectPtr<UMaterialExpressionNamedRerouteDeclaration> BaseNormalReroute;
	TObjectPtr<UMaterialExpressionNamedRerouteDeclaration> TangentReroute;
	TObjectPtr<UMaterialExpressionNamedRerouteDeclaration> BitangentReroute;
	TObjectPtr<UMaterialExpressionNamedRerouteDeclaration> CameraVectorReroute;
	TObjectPtr<UMaterialExpressionNamedRerouteDeclaration> PositionReroute;

	UMaterialExpression* MakeTextureSamplerCustomNode(int Index);

	UMaterialExpression* MakeSynthesizeTangentCustomNode();
};

struct HLSLFunctionCallNodeBuilder
{
public:
	HLSLFunctionCallNodeBuilder(FString FunctionName, FString IncludePath);

	void SetReturnType(ECustomMaterialOutputType ReturnType);

	void InArgument(FString ArgName, UMaterialExpression* OutExpression, int32 OutIndex);
	void InArgument(FString ArgName, FString ArgValue);
	void OutArgument(FString ArgName, ECustomMaterialOutputType OutType);

	UMaterialExpression* Build(FTextureSetMaterialGraphBuilder& GraphBuilder);

private:
	FString FunctionName;
	FString IncludePath;
	ECustomMaterialOutputType ReturnType;

	TArray<FString> FunctionArguments;

	typedef TTuple<FString, UMaterialExpression*, int32> InputConnection;
	TArray<InputConnection> InputConnections;

	typedef TTuple<FString, ECustomMaterialOutputType> Output;
	TArray<Output> Outputs;

	struct NodeConnection
	{
	public:
		NodeConnection(UMaterialExpression* OutExpression, int32 OutIndex, UMaterialExpression* InExpression, int32 InIndex)
			: OutExpression(OutExpression)
			, OutIndex(OutIndex)
			, InExpression(InExpression)
			, InIndex(InIndex)
		{}

		void Connect() const
		{
			OutExpression->ConnectExpression(InExpression->GetInput(InIndex), OutIndex);
		}

		UMaterialExpression* OutExpression;
		int32 OutIndex;
		UMaterialExpression* InExpression;
		int32 InIndex;
	};
};
#endif // WITH_EDITOR
