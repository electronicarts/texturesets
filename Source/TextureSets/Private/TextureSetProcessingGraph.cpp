// (c) Electronic Arts. All Rights Reserved.

#if WITH_EDITOR

#include "TextureSetProcessingGraph.h"

#include "TextureSetModule.h"

FTextureSetProcessingGraph::FTextureSetProcessingGraph()
	: TextureFlags(ETextureSetTextureFlags::None)
	, bHasGenerated(false)
{
}

FTextureSetProcessingGraph::FTextureSetProcessingGraph(const TArray<const UTextureSetModule*>& Modules)
	: FTextureSetProcessingGraph()
{
	Regenerate(Modules);
}

void FTextureSetProcessingGraph::Regenerate(const TArray<const UTextureSetModule*>& Modules)
{
	InputTextures.Empty();
	OutputTextures.Empty();
	OutputParameters.Empty();
	TextureFlags = ETextureSetTextureFlags::None;

	for (const UTextureSetModule* Module : Modules)
	{
		if (IsValid(Module))
			Module->ConfigureProcessingGraph(*this);
	}

	bHasGenerated = true;
}

const TMap<FName, const ITextureProcessingNode*> FTextureSetProcessingGraph::GetOutputTextures() const
{
	// Copy our array into a const array
	TMap<FName, const ITextureProcessingNode*> Result;
	Result.Reserve(OutputTextures.Num());
	for (auto& [Name, Node] : OutputTextures)
		Result.Add(Name, &Node.Get());

	return Result;
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

void FTextureSetProcessingGraph::SetTextureFlagDefault(ETextureSetTextureFlags NewFlags)
{
	checkf(!(TextureFlags & NewFlags), TEXT("Texture flag has already been set by another module, and intended usage may conflict."));
	TextureFlags |= NewFlags;
}
#endif
