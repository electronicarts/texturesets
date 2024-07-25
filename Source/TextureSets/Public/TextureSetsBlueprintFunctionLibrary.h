// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSet.h"
#include "UObject/Object.h"
#include "TextureSetsBlueprintFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UTextureSetsBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Sets a texture parameter value on a dynamic material instance, similar to the built-in SetTextureParameterValue, etc
	 * @param	MID	The dynamic material instance on which to set the parameter.
	 * @param	ParameterInfo	Information about which texture set parameter in MID is to be changed.
	 * @param	Value	The texture set value to set on the parameter of MID.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material|Texture Sets")
	static void SetTextureSetParameterValue(
		UMaterialInstanceDynamic* MID,
		const FMaterialParameterInfo& ParameterInfo,
		UTextureSet* Value);

private:
	// Set the parameters on the MI derived from the texture set parameter (Vector, Texture parameters)
	static void SetDerivedTextureSetParameters(
		UMaterialInstanceDynamic* MID,
		const FMaterialParameterInfo& ParameterInfo,
		UTextureSet* NewTextureSetValue);
	
};
