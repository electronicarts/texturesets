// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MaterialEditor/DEditorCustomParameterValue.h"

#include "DEditorTextureSetParameterValue.generated.h"

class UTextureSet;


#if WITH_EDITORONLY_DATA
UCLASS()
class UDEditorTextureSetParameterValue : public UDEditorCustomParameterValue
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=DEditorTextureParameterValue)
	TObjectPtr<UTextureSet> ParameterValue;

	virtual FName GetDefaultGroupName() const override { return TEXT("Texture Set Parameter Values"); }

	virtual bool GetValue(FMaterialParameterMetadata& OutResult) const override;
	virtual bool SetValue(const FMaterialParameterValue& Value) override;
};
#endif
