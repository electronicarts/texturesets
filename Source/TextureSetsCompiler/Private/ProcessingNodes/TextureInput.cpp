// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "ProcessingNodes/TextureInput.h"

#include "ProcessingNodes/TextureRead.h"
#include "TextureSetProcessingGraph.h"

FTextureInput::FTextureInput(FName SourceNameIn, const FTextureSetSourceTextureDef& SourceDefinitionIn)
	: LastNode(nullptr)
	, TextureRead(MakeShared<FTextureRead>(SourceNameIn, SourceDefinitionIn))
{
}

void FTextureInput::ComputeGraphHash(FHashBuilder& HashBuilder) const
{
	HashBuilder << GetNodeTypeName();

	check(LastNode);
	LastNode->ComputeGraphHash(HashBuilder);
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
