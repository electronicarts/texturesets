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
		{
			WorkingModule = Module;
			Module->ConfigureProcessingGraph(*this);
		}
	}
	WorkingModule = nullptr;

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
	check(IsValid(WorkingModule)); // Inputs should only be added by modules

	if (InputOwners.Contains(Name))
	{
		LogError(FText::Format(
			INVTEXT("Input {0} has already been defined by module {1}"),
			FText::FromName(Name),
			FText::FromString(InputOwners.FindChecked(Name)->GetInstanceName())
		));
	}

	TSharedRef<FTextureInput> NewInput(new FTextureInput(Name, SourceDef));
	InputTextures.Add(Name, NewInput);
	InputOwners.Add(Name, WorkingModule);
	return NewInput;
}

void FTextureSetProcessingGraph::AddOutputTexture(FName Name, TSharedRef<ITextureProcessingNode> Texture)
{
	check(bIsGenerating); // Not valid to add outputs after the graph has finished generating
	check(IsValid(WorkingModule)); // Outputs should only be added by modules

	if (OutputOwners.Contains(Name))
	{
		LogError(FText::Format(
			INVTEXT("Output {0} has already been defined by module {1}"),
			FText::FromName(Name),
			FText::FromString(OutputOwners.FindChecked(Name)->GetInstanceName())
		));
	}

	OutputTextures.Add(Name, Texture);
	OutputOwners.Add(Name, WorkingModule);
}

void FTextureSetProcessingGraph::AddOutputParameter(FName Name, TSharedRef<IParameterProcessingNode> Parameter)
{
	check(bIsGenerating); // Not valid to add outputs after the graph has finished generating
	check(IsValid(WorkingModule)); // Outputs should only be added by modules

	if (OutputOwners.Contains(Name))
	{
		LogError(FText::Format(
			INVTEXT("Output {0} has already been defined by module {1}"),
			FText::FromName(Name),
			FText::FromString(OutputOwners.FindChecked(Name)->GetInstanceName())
		));
	}

	OutputParameters.Add(Name, Parameter);
	OutputOwners.Add(Name, WorkingModule);
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

void FTextureSetProcessingGraph::LogError(FText ErrorText)
{
	Errors.Add(WorkingModule ? FText::Format(INVTEXT("{0}: {1}"), FText::FromString(WorkingModule->GetInstanceName()), ErrorText) : ErrorText);
}
#endif
