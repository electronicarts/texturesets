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
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Name;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UTextureSet> DefaultTextureSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UTextureSetDefinition> Definition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSamplePackedTextures; // Temp, set to true to enable packing and unpacking

	UPROPERTY(EditAnywhere, EditFixedSize, NoClear)
	TArray<class UTextureSetSampleParams*> SampleParams;

	template <class T>
	const T* GetSampleParams() const
	{
		for (UTextureSetSampleParams* Params : SampleParams)
		{
			const T* SP = Cast<T>(Params);

			if (IsValid(SP))
			{
				return SP;
			}
		}
		return GetDefault<T>(); // Not found, return the default class
	}

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

