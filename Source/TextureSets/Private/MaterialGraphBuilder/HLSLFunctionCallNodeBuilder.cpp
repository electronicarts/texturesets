// (c) Electronic Arts. All Rights Reserved.

#include "MaterialGraphBuilder/HLSLFunctionCallNodeBuilder.h"

#if WITH_EDITOR
#include "TextureSetMaterialGraphBuilder.h"

HLSLFunctionCallNodeBuilder::HLSLFunctionCallNodeBuilder(FString FunctionName, FString IncludePath)
	: FunctionName(FunctionName)
	, IncludePath(IncludePath)
	, ReturnType(ECustomMaterialOutputType::CMOT_MAX)
{

}

void HLSLFunctionCallNodeBuilder::SetReturnType(ECustomMaterialOutputType NewReturnType)
{
	ReturnType = NewReturnType;
}

void HLSLFunctionCallNodeBuilder::InArgument(FString ArgName, const FGraphBuilderOutputAddress& Address, const FString Suffix)
{
	FunctionArguments.Add(ArgName + Suffix);
	InputConnections.Add(InputConnection(ArgName, Address.GetExpression(), Address.GetIndex()));
}

void HLSLFunctionCallNodeBuilder::InArgument(FString ArgName, FString ArgValue)
{
	FunctionArguments.Add(FString::Format(TEXT("/*{0}*/ {1}"), {ArgName, ArgValue}));
}

void HLSLFunctionCallNodeBuilder::OutArgument(FString ArgName, ECustomMaterialOutputType OutType)
{
	FunctionArguments.Add(ArgName);
	Outputs.Add(Output(ArgName, OutType));
}

UMaterialExpression* HLSLFunctionCallNodeBuilder::Build(FTextureSetMaterialGraphBuilder* GraphBuilder)
{
	UMaterialExpressionCustom* CustomExp = GraphBuilder->CreateExpression<UMaterialExpressionCustom>();

	CustomExp->Description = FunctionName;

	CustomExp->Inputs.Empty(); // required: class initializes with one input by default

	for (int i = 0; i < InputConnections.Num(); i++)
	{
		FCustomInput CustomInput;
		CustomInput.InputName = FName(InputConnections[i].Get<FString>());
		CustomExp->Inputs.Add(CustomInput);
	}

	CustomExp->OutputType = ReturnType;

	for (int i = 0; i < Outputs.Num(); i++)
	{
		FCustomOutput CustomInput;
		CustomInput.OutputName = FName(Outputs[i].Get<FString>());
		CustomInput.OutputType = Outputs[i].Get<ECustomMaterialOutputType>();
		CustomExp->AdditionalOutputs.Add(CustomInput);
	}

	CustomExp->RebuildOutputs();

	for (int i = 0; i < InputConnections.Num(); i++)
	{
		UMaterialExpression* SourceConnection = InputConnections[i].Get<UMaterialExpression*>();
		const int32& SourceIndex = InputConnections[i].Get<int32>();
		SourceConnection->ConnectExpression(CustomExp->GetInput(i), SourceIndex);
	}

	CustomExp->IncludeFilePaths.Add(IncludePath);
	CustomExp->Code = FString::Format(TEXT("{0}{1}({2});"),
		{
			ReturnType != ECustomMaterialOutputType::CMOT_MAX ? "return " : "",
			FunctionName,
			FString::Join(FunctionArguments, TEXT(", "))
		});

	// Custom exp nodes always have to return something, so just return 0.
	if (ReturnType == ECustomMaterialOutputType::CMOT_MAX)
		CustomExp->Code += "\nreturn 0;";

	return CustomExp;
}

#endif // WITH_EDITOR
