// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

class UTextureSetModule;
class FTextureOperator;
class FTextureInput;
struct FTextureSetSourceTextureDef;
class ITextureProcessingNode;
class IParameterProcessingNode;

#define CreateOperatorFunc TFunction<TSharedRef<ITextureProcessingNode>(TSharedRef<ITextureProcessingNode>)>

class FTextureSetProcessingGraph
{
public:
	FTextureSetProcessingGraph();
	FTextureSetProcessingGraph(const TArray<const UTextureSetModule*>& Modules);

	bool HasGenerated() const { return bHasGenerated; }
	void Regenerate(const TArray<const UTextureSetModule*>& Modules);

	TSharedRef<FTextureInput> AddInputTexture(FName Name, const FTextureSetSourceTextureDef& SourceDef);
	const TMap<FName, TSharedRef<FTextureInput>>& GetInputTextures() const { return InputTextures; }

	void AddOutputTexture(FName Name, TSharedRef<ITextureProcessingNode> Texture) { OutputTextures.Add(Name, Texture); }
	const TMap<FName, TSharedRef<ITextureProcessingNode>>& GetOutputTextures() const { return OutputTextures; }

	void AddOutputParameter(FName Name, TSharedRef<IParameterProcessingNode> Parameter) { OutputParameters.Add(Name, Parameter); }
	const TMap<FName, TSharedRef<IParameterProcessingNode>>& GetOutputParameters() { return OutputParameters; }
	const TMap<FName, const IParameterProcessingNode*> GetOutputParameters() const;

	void AddDefaultInputOperator(CreateOperatorFunc Func) { DefaultInputOperators.Add(Func); }
	const TArray<CreateOperatorFunc>& GetDefaultInputOperators() const { return DefaultInputOperators; }

private:
	TMap<FName, TSharedRef<FTextureInput>> InputTextures;
	TArray<CreateOperatorFunc> DefaultInputOperators;
	TMap<FName, TSharedRef<ITextureProcessingNode>> OutputTextures;
	TMap<FName, TSharedRef<IParameterProcessingNode>> OutputParameters;

	bool bHasGenerated;
	bool bIsGenerating;
};
#endif
