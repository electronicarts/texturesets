// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "ProceduralMaterialFunction.h"

#include "MaterialExpressionTextureSetSampleParameter.generated.h"

class UTextureSet;
class FObjectInitializer;
class FArguments;

UCLASS(HideCategories = (MaterialExpressionMaterialFunctionCall))
class TEXTURESETS_API UMaterialExpressionTextureSetSampleParameter : public UProceduralMaterialFunction
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(EditAnywhere)
	FName ParameterName;
	
	UPROPERTY(EditAnywhere)
	FName ParameterGroupName;

	UPROPERTY(EditAnywhere)
	TObjectPtr<class UTextureSet> DefaultTextureSet;

	UPROPERTY(EditAnywhere)
	TObjectPtr<class UTextureSetDefinition> Definition;

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

	FName GetTextureParameterName(int TextureIndex) const;
	FName GetConstantParameterName(FName ConstantName) const;

	static FName MakeTextureParameterName(FName ParameterName, int TextureIndex);
	static FName MakeConstantParameterName(FName ParameterName, FName ConstantName);
	static bool IsTextureSetParameterName(FName Name);

#if WITH_EDITOR
	virtual class UMaterialFunction* CreateMaterialFunction() override;
#endif

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) override;
#endif

private:
#if WITH_EDITOR
	void UpdateSampleParamArray();
#endif

};
