//
// (c) Electronic Arts.  All Rights Reserved.
//

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"

#include "MaterialExpressionTextureSetSampleParameter.generated.h"

class UTextureSet;
class UMaterialExpressionTextureObjectParameter;
class FObjectInitializer;
class FArguments;

UMaterialGraphNode* AddExpression(UMaterialExpression* Expression, UMaterialGraph* Graph);

UCLASS(MinimalAPI)
class UMaterialExpressionTextureSetSampleParameter : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(meta = (RequiredInput = "true", ToolTip = "TODO: Tooltip"))
	FExpressionInput Coordinates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialExpressionTextureBase)
	FName Name;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialExpressionTextureBase)
	TObjectPtr<class UTextureSet> TextureSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialExpressionTextureBase)
	TObjectPtr<class UTextureSetDefinition> TextureSetData;

	UPROPERTY(meta = (RequiredInput = "true", ToolTip = "TODO: Tooltip"))
	TArray<FExpressionInput> TextureReferenceInputs;

#if WITH_EDITOR

	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void  GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FText GetKeywords() const override;

	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
	
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;

	virtual const TArray<FExpressionInput*> GetInputs() override;

#endif

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PostEditUndo() override;

private:

	void SetupOutputs();

public:
	void BuildTextureParameterChildren();

};

UCLASS()
class TEXTURESETS_API UMaterialExpressionTextureReferenceInternal : public UMaterialExpressionTextureObjectParameter
{
	GENERATED_BODY()
};

