// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTextureSetModule;
class FTextureOperator;
class FTextureInput;
struct FTextureSetSourceTextureDef;
class ITextureProcessingNode;
class IParameterProcessingNode;

#define CreateOperatorFunc TFunction<TSharedRef<ITextureProcessingNode>(TSharedRef<ITextureProcessingNode>)>

class TEXTURESETSCOMPILER_API FTextureSetProcessingGraph
{
public:
	FTextureSetProcessingGraph();
	FTextureSetProcessingGraph(const TArray<const UTextureSetModule*>& Modules);

	bool HasGenerated() const { return bHasGenerated; }
	void Regenerate(const TArray<const UTextureSetModule*>& Modules);

	TSharedRef<FTextureInput> AddInputTexture(FName Name, const FTextureSetSourceTextureDef& SourceDef);
	const TMap<FName, TSharedRef<FTextureInput>>& GetInputTextures() const { return InputTextures; }

	void AddOutputTexture(FName Name, TSharedRef<ITextureProcessingNode> Texture, bool bAllowOverride = false);
	const TMap<FName, TSharedRef<ITextureProcessingNode>>& GetOutputTextures() const { return OutputTextures; }

	void AddOutputParameter(FName Name, TSharedRef<IParameterProcessingNode> Parameter);
	const TMap<FName, TSharedRef<IParameterProcessingNode>>& GetOutputParameters() { return OutputParameters; }
	const TMap<FName, const IParameterProcessingNode*> GetOutputParameters() const;

	void AddDefaultInputOperator(CreateOperatorFunc Func) { DefaultInputOperators.Add(Func); }
	const TArray<CreateOperatorFunc>& GetDefaultInputOperators() const { return DefaultInputOperators; }

	void LogError(FText ErrorText);
	const TArray<FText>& GetErrors() const { return Errors; }

private:
	TMap<FName, TSharedRef<FTextureInput>> InputTextures;
	TArray<CreateOperatorFunc> DefaultInputOperators;
	TMap<FName, TSharedRef<ITextureProcessingNode>> OutputTextures;
	TMap<FName, TSharedRef<IParameterProcessingNode>> OutputParameters;

	TMap<FName, const UTextureSetModule*> InputOwners;
	TMap<FName, const UTextureSetModule*> OutputOwners;

	bool bHasGenerated;
	bool bIsGenerating;

	TArray<FText> Errors;

	// The module that is currently executing during generation (if any)
	// Should mainly be used for error reporting and validation
	const UTextureSetModule* WorkingModule;
};
