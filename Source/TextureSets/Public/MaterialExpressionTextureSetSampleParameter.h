// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMaterialFunction.h"
#include "TextureSetAssetParams.h"
#include "TextureSetSampleContext.h"

#include "MaterialExpressionTextureSetSampleParameter.generated.h"

class UTextureSet;
class FObjectInitializer;
class FArguments;

UCLASS(HideCategories = (MaterialExpressionMaterialFunctionCall))
class TEXTURESETS_API UMaterialExpressionTextureSetSampleParameter : public UProceduralMaterialFunction
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Parameter")
	FName ParameterName;
	
	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY(VisibleAnywhere, Category="Parameter")
	FGuid ExpressionGUID;

	/** The name of the parameter Group to display in MaterialInstance Editor. Default is None group */
	UPROPERTY(EditAnywhere, Category="Parameter")
	FName Group;

	/** Controls where the this parameter is displayed in a material instance parameter list.  The lower the number the higher up in the parameter list. */
	UPROPERTY(EditAnywhere, Category="Parameter")
	int32 SortPriority = 32;

	UPROPERTY(EditAnywhere, Category="Parameter")
	TSoftObjectPtr<class UTextureSet> DefaultTextureSet;

	UPROPERTY(EditAnywhere, Category="Parameter")
	TObjectPtr<class UTextureSetDefinition> Definition;

	UPROPERTY(EditAnywhere, Category="Parameter")
	FTextureSetAssetParamsCollection SampleParams;

	// TODO: Replace with FTextureSetSampleContext and/or make FTextureSetSampleContext a SampleParam (will require data fixup)
	UPROPERTY(EditAnywhere, Category="Context")
	EBaseNormalSource BaseNormalSource = EBaseNormalSource::Vertex;

	UPROPERTY(EditAnywhere, Category="Context")
	ETangentSource TangentSource = ETangentSource::Synthesized;

	UPROPERTY(EditAnywhere, Category="Context")
	EPositionSource PositionSource = EPositionSource::World;

	UPROPERTY(EditAnywhere, Category="Context")
	ECameraVectorSource CameraVectorSource = ECameraVectorSource::World;

	// UProceduralMaterialFunction Interface
#if WITH_EDITOR
	virtual uint32 ComputeMaterialFunctionHash() override;
	virtual bool ConfigureMaterialFunction(class UMaterialFunction* NewMaterialFunction) override;
#endif

	// UMaterialExpression Interface
	virtual FGuid& GetParameterExpressionId() override;
#if WITH_EDITOR
	virtual FName GetParameterName() const override;
	virtual void SetParameterName(const FName& Name) override;
	virtual bool GetParameterValue(FMaterialParameterMetadata& OutMeta) const override;
	virtual bool SetParameterValue(const FName& Name, const FMaterialParameterMetadata& Meta, EMaterialExpressionSetParameterValueFlags Flags = EMaterialExpressionSetParameterValueFlags::None) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif

	// UObject Interface
	virtual void BeginDestroy();
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
#if WITH_EDITOR

	FDelegateHandle OnTextureSetDefinitionChangedHandle;

	void UpdateSampleParamArray();

	void OnDefinitionChanged(UTextureSetDefinition* ChangedDefinition);
#endif

	UPROPERTY(VisibleAnywhere, Category="Debug")
	TArray<FText> BuilderErrors;
};
