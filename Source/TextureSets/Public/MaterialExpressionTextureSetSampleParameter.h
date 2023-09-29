// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMaterialFunction.h"
#include "TextureSetAssetParams.h"

#include "MaterialExpressionTextureSetSampleParameter.generated.h"

class UTextureSet;
class FObjectInitializer;
class FArguments;

UENUM()
enum class EBaseNormalSource : uint8
{
	Explicit UMETA(ToolTip = "Explicitly define base normal input."),
	Vertex UMETA(ToolTip = "Use the mesh's vertex normal as the base normal."),
};

UENUM()
enum class ETangentSource : uint8
{
	Explicit UMETA(ToolTip = "Explicitly define tangent and bitangent inputs."),
	Synthesized UMETA(ToolTip = "Synthesized the tangent and bitangent with screen-space derivatives based on input UVs, base normal, and position."),
	Vertex UMETA(ToolTip = "Read the tangent and bitangent from the mesh's vertex data. More accurate than Synthesized tangents, but only works correctly if your input UVs are based on the UV0 of the mesh!"),
};

UENUM()
enum class EPositionSource : uint8
{
	Explicit UMETA(ToolTip = "Explicitly position to use for internal calculations."),
	World UMETA(ToolTip = "Use the world position if a position is needed for any internal calculations."),
};

UENUM()
enum class ECameraVectorSource : uint8
{
	Explicit UMETA(ToolTip = "Explicitly camera vector to use for internal calculations."),
	World UMETA(ToolTip = "Use the world camera vector if it's needed for any internal calculations."),
};


UCLASS(HideCategories = (MaterialExpressionMaterialFunctionCall))
class TEXTURESETS_API UMaterialExpressionTextureSetSampleParameter : public UProceduralMaterialFunction
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(EditAnywhere)
	FName ParameterName;
	
	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY(VisibleAnywhere)
	FGuid ExpressionGUID;

	/** The name of the parameter Group to display in MaterialInstance Editor. Default is None group */
	UPROPERTY(EditAnywhere)
	FName Group;

	/** Controls where the this parameter is displayed in a material instance parameter list.  The lower the number the higher up in the parameter list. */
	UPROPERTY(EditAnywhere)
	int32 SortPriority = 32;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<class UTextureSet> DefaultTextureSet;

	UPROPERTY(EditAnywhere)
	TObjectPtr<class UTextureSetDefinition> Definition;

	UPROPERTY(EditAnywhere)
	FTextureSetAssetParamsCollection SampleParams;

	UPROPERTY(EditAnywhere, Category=Context)
	EBaseNormalSource BaseNormalSource = EBaseNormalSource::Vertex;

	UPROPERTY(EditAnywhere, Category=Context)
	ETangentSource TangentSource = ETangentSource::Synthesized;

	UPROPERTY(EditAnywhere, Category=Context)
	EPositionSource PositionSource = EPositionSource::World;

	UPROPERTY(EditAnywhere, Category=Context)
	ECameraVectorSource CameraVectorSource = ECameraVectorSource::World;

	FName GetTextureParameterName(int TextureIndex) const;
	FName GetConstantParameterName(FName ConstantName) const;

	static FName MakeTextureParameterName(FName ParameterName, int TextureIndex);
	static FName MakeConstantParameterName(FName ParameterName, FName ConstantName);
	static bool IsTextureSetParameterName(FName Name);

	// UProceduralMaterialFunction Interface
#if WITH_EDITOR
	virtual uint32 ComputeMaterialFunctionHash() override;
	virtual void ConstructMaterialFunction(class UMaterialFunction* NewMaterialFunction) override;
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
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) override;
#endif

private:
#if WITH_EDITOR

	FDelegateHandle OnTextureSetDefinitionChangedHandle;

	void UpdateSampleParamArray();

	void OnDefinitionChanged(UTextureSetDefinition* ChangedDefinition);
#endif
};
