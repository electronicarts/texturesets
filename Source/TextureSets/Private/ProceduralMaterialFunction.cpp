// Copyright (c) 2024 Electronic Arts. All Rights Reserved.


#include "ProceduralMaterialFunction.h"

#include "MaterialCompiler.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraphNode.h"
#endif

UProceduralMaterialFunction::UProceduralMaterialFunction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaterialFunctionHash = 0;
}

void UProceduralMaterialFunction::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	UpdateMaterialFunction();
#endif
}

#if WITH_EDITOR
void UProceduralMaterialFunction::PostEditImport()
{
	Super::PostEditImport();

	MaterialFunction = nullptr;
	MaterialFunctionHash = 0;
	UpdateMaterialFunction();
}
#endif

#if WITH_EDITOR
void UProceduralMaterialFunction::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet || PropertyChangedEvent.ChangeType == EPropertyChangeType::Unspecified )
		UpdateMaterialFunction();
}
#endif

#if WITH_EDITOR
int32 UProceduralMaterialFunction::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	FDataValidationContext ValidationContext;
	EDataValidationResult ValidationResult = IsDataValid(ValidationContext);

	if(ValidationResult == EDataValidationResult::Invalid)
	{
		for (auto Issue : ValidationContext.GetIssues())
		{
			if (Issue.Severity == EMessageSeverity::Type::Error)
			{
				return Compiler->Errorf(*Issue.Message.ToString());
			}
		}
	}

	return Super::Compile(Compiler, OutputIndex);
}
#endif

#if WITH_EDITOR
bool UProceduralMaterialFunction::SetMaterialFunction(UMaterialFunctionInterface* NewMaterialFunction)
{
	// Do not allow anyone external to directly set the material function.
	// It should be set only via UpdateMaterialFunction
	return false;
}
#endif

#if WITH_EDITOR
bool UProceduralMaterialFunction::UpdateMaterialFunction()
{
	uint32 NewHash = HashCombine(ComputeMaterialFunctionHash(), GetTypeHash(FString("ProceduralMaterialFunction_V1")));

	if (bSuccessfullyConfigured && IsValid(MaterialFunction) && (MaterialFunctionHash == NewHash))
	{
		// Nothing needs updating
		return false;
	}

	FName FunctionName = MakeUniqueObjectName(GetOutermostObject(), UMaterialFunction::StaticClass());
	EObjectFlags Flags = RF_Public;

	UMaterialFunction* NewMaterialFunction = NewObject<UMaterialFunction>(GetOutermostObject(), FunctionName, Flags);
	bSuccessfullyConfigured = ConfigureMaterialFunction(NewMaterialFunction);

	if (!bSuccessfullyConfigured)
	{
		// Abort, leaving the old function in place as to not break any connections until it succeeds.
		return false;
	}

	MaterialFunction = NewMaterialFunction;

	MaterialFunctionHash = NewHash;

	// Fix up Expression input/output references, since while the "FunctionInputs" and "FunctionOutputs" are serialized,
	// the UMaterialFunction is not, and thus the ExpressionInput/Output values are null.
	for (UMaterialExpression* CurrentExpression : NewMaterialFunction->GetExpressions())
	{
		UMaterialExpressionFunctionOutput* OutputExpression = Cast<UMaterialExpressionFunctionOutput>(CurrentExpression);
		UMaterialExpressionFunctionInput* InputExpression = Cast<UMaterialExpressionFunctionInput>(CurrentExpression);

		if (InputExpression)
		{
			for (FFunctionExpressionInput& Input : FunctionInputs)
			{
				if (InputExpression->InputName == Input.Input.InputName)
				{
					Input.ExpressionInput = InputExpression;
					Input.ExpressionInputId = InputExpression->Id;
				}
			}
		}
		else if (OutputExpression)
		{
			for (FFunctionExpressionOutput& Output : FunctionOutputs)
			{
				if (OutputExpression->OutputName == Output.Output.OutputName)
				{
					Output.ExpressionOutput = OutputExpression;
					Output.ExpressionOutputId = OutputExpression->Id;
				}
			}
		}
	}

	// Remove Inputs that don't reference a valid expression
	for (int i = 0; i < FunctionInputs.Num(); i++)
	{
		if (!IsValid(FunctionInputs[i].ExpressionInput))
		{
			FunctionInputs.RemoveAt(i);
			i--;
		}
	}

	// Remove Outputs that don't reference a valid expression
	for (int i = 0; i < FunctionOutputs.Num(); i++)
	{
		if (!IsValid(FunctionOutputs[i].ExpressionOutput))
		{
			FunctionOutputs.RemoveAt(i);
			i--;
		}
	}

	UpdateFromFunctionResource(true);

	return true;
}
#endif
