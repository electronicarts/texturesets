// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "Materials/MaterialExpressionCustom.h"
#include "MaterialGraphBuilder/GraphBuilderGraphAddress.h"

class FTextureSetMaterialGraphBuilder;

#if WITH_EDITOR
struct HLSLFunctionCallNodeBuilder
{
public:
	HLSLFunctionCallNodeBuilder(FString FunctionName, FString IncludePath);

	void SetReturnType(ECustomMaterialOutputType ReturnType);

	void InArgument(FString ArgName, const FGraphBuilderOutputAddress& Address, const FString Suffix = "");
	void InArgument(FString ArgName, FString ArgValue);
	void OutArgument(FString ArgName, ECustomMaterialOutputType OutType);

	UMaterialExpression* Build(FTextureSetMaterialGraphBuilder* GraphBuilder);

private:
	FString FunctionName;
	FString IncludePath;
	ECustomMaterialOutputType ReturnType;

	TArray<FString> FunctionArguments;

	typedef TTuple<FString, UMaterialExpression*, int32> InputConnection;
	TArray<InputConnection> InputConnections;

	typedef TTuple<FString, ECustomMaterialOutputType> Output;
	TArray<Output> Outputs;

	struct NodeConnection
	{
	public:
		NodeConnection(UMaterialExpression* OutExpression, int32 OutIndex, UMaterialExpression* InExpression, int32 InIndex)
			: OutExpression(OutExpression)
			, OutIndex(OutIndex)
			, InExpression(InExpression)
			, InIndex(InIndex)
		{}

		void Connect() const
		{
			OutExpression->ConnectExpression(InExpression->GetInput(InIndex), OutIndex);
		}

		UMaterialExpression* OutExpression;
		int32 OutIndex;
		UMaterialExpression* InExpression;
		int32 InIndex;
	};
};
#endif // WITH_EDITOR
