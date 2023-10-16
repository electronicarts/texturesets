// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "MaterialEditingLibrary.h"
#include "MaterialGraphBuilder/GraphBuilderGraphAddress.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "TextureSetInfo.h"

class UTextureSetDefinition;
class UMaterialExpressionTextureSetSampleParameter;
class UMaterialExpressionFunctionOutput;
class UMaterialExpressionTextureObjectParameter;
class UMaterialExpressionNamedRerouteDeclaration;

UENUM()
enum class EGraphBuilderSharedValueType : uint8
{
	// UV used for doing texture reads
	UV_Sampling,
	// UV used to inform the streaming system for non-virtual textures
	UV_Streaming,
	// UV used to look up mips for texture reads
	UV_Mip,
	// DDX and DDY of UV used to look up mips for texture reads
	UV_DDX,
	UV_DDY,
	// Index used for sampling from texture arrays
	ArrayIndex,
	// Base normal used for tangent space transforms
	BaseNormal,
	// Tangent and Bitangent used for tangent space transforms
	Tangent,
	BiTangent,
	// Per-pixel position value. Typically in world space, but also valid to be in view or object
	Position,
	// Pixel to camera vector. Should be in the same space as the position
	CameraVector,
};

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

	FTextureSetMaterialGraphBuilder(UMaterialFunction* MaterialFunction, const UMaterialExpressionTextureSetSampleParameter* Node);

	template <class T> T* CreateExpression()
	{
		return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, T::StaticClass()));
	}

	// Change this to regenerate all texture set material graphs
	static FString GetGraphBuilderVersion() { return "0.1"; }

	const FGraphBuilderOutputAddress& GetSharedValue(EGraphBuilderSharedValueType Value);
	const void SetSharedValue(FGraphBuilderOutputAddress Address, EGraphBuilderSharedValueType Value);

	void Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, UMaterialExpression* InputNode, uint32 InputIndex);
	void Connect(const FGraphBuilderOutputAddress& Output, UMaterialExpression* InputNode, uint32 InputIndex);
	void Connect(UMaterialExpression* OutputNode, uint32 OutputIndex, const FGraphBuilderInputAddress& Input);
	void Connect(const FGraphBuilderOutputAddress& Output, const FGraphBuilderInputAddress& Input);
	
	// This is the texture coordinate input via the UV pin, without any modifications
	const FGraphBuilderOutputAddress& GetRawUV() const;

	const FGraphBuilderOutputAddress GetProcessedTextureSample(FName Name);
	
	UMaterialExpressionTextureObjectParameter* GetPackedTextureObject(int Index);
	const FGraphBuilderOutputAddress GetPackedTextureSize(int Index);

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

	const FGraphBuilderOutputAddress RawUV;

	TMap<EGraphBuilderSharedValueType, FGraphBuilderOutputAddress> SharedValueSources;
	TMap<EGraphBuilderSharedValueType, FGraphBuilderOutputAddress> SharedValueReroute;

	// Key is input address, and value is output
	TMap<FGraphBuilderInputAddress, FGraphBuilderOutputAddress> DeferredConnections;

	void SetupSharedValues();

	UMaterialExpression* MakeTextureSamplerCustomNode(int Index);

	UMaterialExpression* MakeSynthesizeTangentCustomNode();
};
#endif // WITH_EDITOR
