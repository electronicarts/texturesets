// (c) Electronic Arts. All Rights Reserved.

#if WITH_EDITOR

#include "TextureSetProcessingGraph.h"

#include "ProcessingNodes/TextureInput.h"
#include "TextureSetModule.h"

FTextureSetProcessingGraph::FTextureSetProcessingGraph()
	: bHasGenerated(false)
	, bIsGenerating(false)
{
}

FTextureSetProcessingGraph::FTextureSetProcessingGraph(const TArray<const UTextureSetModule*>& Modules)
	: FTextureSetProcessingGraph()
{
	Regenerate(Modules);
}

void FTextureSetProcessingGraph::Regenerate(const TArray<const UTextureSetModule*>& Modules)
{
	bIsGenerating = true;
	InputTextures.Empty();
	DefaultInputOperators.Empty();
	OutputTextures.Empty();
	OutputParameters.Empty();

	for (const UTextureSetModule* Module : Modules)
	{
		if (IsValid(Module))
			Module->ConfigureProcessingGraph(*this);
	}

	for (auto& [InputName, InputTexture] : InputTextures)
	{
		InputTexture->InstantiateOperators(*this);
	}

	bIsGenerating = false;
	bHasGenerated = true;
}

TSharedRef<FTextureInput> FTextureSetProcessingGraph::AddInputTexture(FName Name, const FTextureSetSourceTextureDef& SourceDef)
{
	check(bIsGenerating); // Not valid to add inputs after the graph has finished generating
	TSharedRef<FTextureInput> NewInput(new FTextureInput(Name, SourceDef));
	InputTextures.Add(Name, NewInput);
	return NewInput;
}

const TMap<FName, const IParameterProcessingNode*> FTextureSetProcessingGraph::GetOutputParameters() const
{
	// Copy our array into a const array
	TMap<FName, const IParameterProcessingNode*> Result;
	Result.Reserve(OutputParameters.Num());
	for (auto& [Name, Node] : OutputParameters)
		Result.Add(Name, &Node.Get());

	return Result;
}
#endif
