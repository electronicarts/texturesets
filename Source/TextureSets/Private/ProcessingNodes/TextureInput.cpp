// (c) Electronic Arts. All Rights Reserved.

#if WITH_EDITOR

#include "ProcessingNodes/TextureInput.h"

#include "ProcessingNodes/TextureRead.h"
#include "TextureSetDefinition.h"
#include "TextureSetProcessingGraph.h"

FTextureInput::FTextureInput(FName SourceNameIn, const FTextureSetSourceTextureDef& SourceDefinitionIn)
	: LastNode(nullptr)
	, TextureRead(MakeShared<FTextureRead>(SourceNameIn, SourceDefinitionIn))
{
}

const uint32 FTextureInput::ComputeGraphHash() const
{
	uint32 Hash = GetTypeHash(GetNodeTypeName().ToString());

	check(LastNode);
	Hash = HashCombine(Hash, LastNode->ComputeGraphHash());

	return Hash;
}

void FTextureInput::InstantiateOperators(const FTextureSetProcessingGraph& Graph)
{
	check(LastNode == nullptr); // Should only be called once

	LastNode = TextureRead;

	Operators.Reserve(Graph.GetDefaultInputOperators().Num() + CreateOperatorFuncs.Num());

	for (const CreateOperatorFunc& Func : Graph.GetDefaultInputOperators())
		LastNode = Operators.Add_GetRef(Func(LastNode.ToSharedRef()));

	for (const CreateOperatorFunc& Func : CreateOperatorFuncs)
		LastNode = Operators.Add_GetRef(Func(LastNode.ToSharedRef()));
}
#endif
