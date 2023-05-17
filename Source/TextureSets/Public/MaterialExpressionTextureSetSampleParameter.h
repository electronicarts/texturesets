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

UCLASS(MinimalAPI, HideCategories = (MaterialExpressionMaterialFunctionCall))
class UMaterialExpressionTextureSetSampleParameter : public UMaterialExpressionMaterialFunctionCall
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialExpressionTextureBase)
	FName Name;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialExpressionTextureBase)
	TObjectPtr<class UTextureSet> TextureSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialExpressionTextureBase)
	TObjectPtr<class UTextureSetDefinition> Definition;

	UPROPERTY(EditAnywhere, EditFixedSize)
	TArray<class UTextureSetSampleParams*> SampleParams;

#if WITH_EDITOR

	virtual void  GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FText GetKeywords() const override;

	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
	
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

#endif

	virtual void PostLoad() override;

private:
	void FixupMaterialFunction(TObjectPtr<UMaterialFunction> NewMaterialFunction);
	void SetMaterialFunctionFromDefinition();
	void UpdateSampleParamArray();

};

UCLASS()
class TEXTURESETS_API UMaterialExpressionTextureReferenceInternal : public UMaterialExpressionTextureObjectParameter
{
	GENERATED_BODY()
};

