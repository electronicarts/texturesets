// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "ProceduralMaterialFunction.h"

#include "MaterialExpressionTextureSetSampleParameter.generated.h"

class UTextureSet;
class FObjectInitializer;
class FArguments;

UCLASS(MinimalAPI, HideCategories = (MaterialExpressionMaterialFunctionCall))
class UMaterialExpressionTextureSetSampleParameter : public UProceduralMaterialFunction
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(EditAnywhere)
	FName ParameterName;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<class UTextureSet> DefaultTextureSet;

	UPROPERTY(EditAnywhere)
	TObjectPtr<class UTextureSetDefinition> Definition;

	UPROPERTY(EditAnywhere)
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

	FName GetTextureParameterName(int TextureIndex) const;

	virtual class UMaterialFunction* CreateMaterialFunction() override;

private:
	void UpdateSampleParamArray();

};

UCLASS()
class TEXTURESETS_API UMaterialExpressionTextureReferenceInternal : public UMaterialExpressionTextureObjectParameter
{
	GENERATED_BODY()
};

