// (c) Electronic Arts.  All Rights Reserved.

#pragma once

#include "TextureSetInfo.h"

#if WITH_EDITOR

class UTextureSetDefinition;
class UMaterialExpressionTextureSetSampleParameter;
class UMaterialExpressionFunctionOutput;
class UMaterialExpressionTextureObjectParameter;

// Class responsible for building the material graph of a texture set sampler node.
// Texture set modules use this node to customize the sampling logic.
class FTextureSetMaterialGraphBuilder
{
public:
	FTextureSetMaterialGraphBuilder(UMaterialFunction* MaterialFunction, const UMaterialExpressionTextureSetSampleParameter* Node);

	template <class T> T* CreateExpression()
	{
		return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, T::StaticClass()));
	}

	UMaterialExpression* GetProcessedTextureSample(FName Name);

	UMaterialExpressionFunctionOutput* CreateOutput(FName Name);
	UMaterialExpressionFunctionOutput* GetOutput(FName Name);

	UMaterialExpression* MakeConstantParameter(FName Name, FVector4 Default);

private:
	TObjectPtr<const UMaterialExpressionTextureSetSampleParameter> Node;
	TObjectPtr<const UTextureSetDefinition> Definition;
	TObjectPtr<UMaterialFunction> MaterialFunction;

	const FTextureSetDefinitionModuleInfo ModuleInfo;
	const FTextureSetPackingInfo PackingInfo;

	TArray<TObjectPtr<UMaterialExpressionTextureObjectParameter>> PackedTextureObjects;
	TArray<TObjectPtr<UMaterialExpression>> PackedTextureSamples;

	// Valid for both texture names "BaseColor" and with channel suffix "BaseColor.g"
	TMap<FName, TObjectPtr<UMaterialExpression>> ProcessedTextureSamples;

	TMap<FName, TObjectPtr<UMaterialExpression>> ConstantParameters;

	// Key is the channel you want to find (e.g. "Normal.r" or "Roughness")
	// Value is a tuple of the packed texture and channel where you'll find it
	TMap<FName, TTuple<int, int>> PackingSource;

	TMap<FName, TObjectPtr<UMaterialExpressionFunctionOutput>> SampleOutputs;

	UMaterialExpression* MakeTextureSamplerCustomNode(UMaterialExpression* Texcoord, int Index);
};
#endif // WITH_EDITOR
