// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "ProceduralMaterialFunction.h"
#if WITH_EDITOR
#include "MaterialEditor/DEditorCustomParameterValue.h"
#endif

#include "MaterialExpressionTextureSetSampleParameter.generated.h"

class UTextureSet;
class FObjectInitializer;
class FArguments;

#if WITH_EDITORONLY_DATA
UCLASS()
class TEXTURESETS_API UDEditorTextureSetParameterValue : public UDEditorCustomParameterValue
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=DEditorTextureParameterValue)
	TObjectPtr<UTextureSet> ParameterValue;

	virtual FName GetDefaultGroupName() const override { return TEXT("Texture Set Parameter Values"); }

	virtual bool GetValue(FMaterialParameterMetadata& OutResult) const override;
	virtual bool SetValue(const FMaterialParameterValue& Value) override;
};
#endif

UCLASS(HideCategories = (MaterialExpressionMaterialFunctionCall))
class TEXTURESETS_API UMaterialExpressionTextureSetSampleParameter : public UProceduralMaterialFunction
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(EditAnywhere)
	FName ParameterName;
	
	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY()
	FGuid ExpressionGUID;

	/** The name of the parameter Group to display in MaterialInstance Editor. Default is None group */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionParameter)
	FName Group;

	/** Controls where the this parameter is displayed in a material instance parameter list.  The lower the number the higher up in the parameter list. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionParameter)
	int32 SortPriority = 32;

	static FName ParameterDefaultName;

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

	// UProceduralMaterialFunction Interface
#if WITH_EDITOR
	virtual class UMaterialFunction* CreateMaterialFunction() override;
#endif

	// UMaterialExpression Interface
	virtual FGuid& GetParameterExpressionId() override;
#if WITH_EDITOR
	virtual FName GetParameterName() const override;
	virtual void SetParameterName(const FName& Name) override;
	virtual bool GetParameterValue(FMaterialParameterMetadata& OutMeta) const override;
	virtual bool SetParameterValue(const FName& Name, const FMaterialParameterMetadata& Meta, EMaterialExpressionSetParameterValueFlags Flags = EMaterialExpressionSetParameterValueFlags::None) override;
#endif

	// UObject Interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) override;
#endif

private:
#if WITH_EDITOR
	void UpdateSampleParamArray();
#endif

};
