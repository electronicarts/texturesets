// (c) Electronic Arts. All Rights Reserved.

#include "MaterialGraphBuilder/GraphBuilderGraphAddress.h"

#if WITH_EDITOR
#include "Materials/MaterialExpression.h"

bool FGraphBuilderInputAddress::IsValid() const
{
	return GetExpression() && GetExpression()->GetInput(GetIndex()) != nullptr;
}

bool FGraphBuilderOutputAddress::IsValid() const
{
	return GetExpression() && GetIndex() < GetExpression()->GetOutputs().Num();
}
#endif // WITH_EDITOR
