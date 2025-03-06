// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "GraphBuilderPin.h"

#include "Materials/MaterialExpression.h"

bool FGraphBuilderInputPin::IsValid() const
{
	return GetExpression() && GetExpression()->GetInput(GetIndex()) != nullptr;
}

bool FGraphBuilderOutputPin::IsValid() const
{
	return GetExpression() && GetIndex() < GetExpression()->GetOutputs().Num();
}
