// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "GraphBuilderGraphAddress.h"

#include "Materials/MaterialExpression.h"

bool FGraphBuilderInputAddress::IsValid() const
{
	return GetExpression() && GetExpression()->GetInput(GetIndex()) != nullptr;
}

bool FGraphBuilderOutputAddress::IsValid() const
{
	return GetExpression() && GetIndex() < GetExpression()->GetOutputs().Num();
}
