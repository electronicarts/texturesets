// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetsBlueprintFunctionLibrary.h"

#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetsHelpers.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"

void UTextureSetsBlueprintFunctionLibrary::SetTextureSetParameterValue(
	UMaterialInstanceDynamic* MID,
	const FMaterialParameterInfo& ParameterInfo,
	UTextureSet* Value)
{
	if (!IsValid(Value) || !IsValid(MID))
	{
		return;
	}
	
	FMaterialParameterMetadata CustomParameterMetadata;
	bool bParameterFound = MID->GetParameterValue(EMaterialParameterType::Custom, ParameterInfo, CustomParameterMetadata, EMaterialGetParameterValueFlags::CheckAll);
	if (!bParameterFound)
	{
		FString ScriptCallstack = FFrame::GetScriptCallstack();
		ensureAlwaysMsgf(
			false,
			TEXT("[Set Texture Set Parameter Value] called with MID = %s, which has no parameter matching ParameterInfo = %s.\n\nCalled from %s"),
				*MID->GetName(),
				*ParameterInfo.ToString(),
				*FFrame::GetScriptCallstack());
		return;
	}

	ensure(IsValid(CustomParameterMetadata.Value.AsCustomObject()));	
	
	TObjectPtr<UObject> CustomParameterObject = CustomParameterMetadata.Value.AsCustomObject();
	TObjectPtr<UTextureSet> TextureSet = CastChecked<UTextureSet>(CustomParameterObject);

	if (TextureSet->Definition != Value->Definition)
	{
		FString ScriptCallstack = FFrame::GetScriptCallstack();
		ensureAlwaysMsgf(
			false,
			TEXT("[Set Texture Set Parameter Value] called with Value = %s, which has TSD %s. This does not match TSD %s for Parameter = %s on MID = %s.\n\nCalled from %s"),
				*Value->GetName(),
				*Value->Definition->GetName(),
				*TextureSet->Definition->GetName(),
				*ParameterInfo.ToString(),
				*MID->GetName(),
				*FFrame::GetScriptCallstack());
		return;
	}
	
	UObject* UObjectValue = Cast<UObject>(Value);
	MID->SetCustomParameterValue(ParameterInfo, UObjectValue);

	SetDerivedTextureSetParameters(MID, ParameterInfo, Value);
}

void UTextureSetsBlueprintFunctionLibrary::SetDerivedTextureSetParameters(
	UMaterialInstanceDynamic* MID,
	const FMaterialParameterInfo& ParameterInfo,
	UTextureSet* NewTextureSetValue)
{
	FMaterialParameterMetadata ParameterMetadata;
	
	bool bParameterFound = MID->GetParameterValue(EMaterialParameterType::Custom, ParameterInfo, ParameterMetadata);
	ensure(bParameterFound);
	
	TArray<FTextureParameterValue> CustomTextureParameterValues;
	TArray<FVectorParameterValue> CustomVectorParameterValues;

	if (NewTextureSetValue != nullptr)
	{
		FCustomParameterValue CustomParameterValue(ParameterInfo, NewTextureSetValue);
		NewTextureSetValue->AugmentMaterialParameters(CustomParameterValue, CustomTextureParameterValues);
		NewTextureSetValue->AugmentMaterialParameters(CustomParameterValue, CustomVectorParameterValues);
	}
	
	for (FTextureParameterValue& TextureParameterValue : CustomTextureParameterValues)
	{
		MID->SetTextureParameterValue(TextureParameterValue.ParameterInfo.Name, TextureParameterValue.ParameterValue);
	}
	
	for (FVectorParameterValue& VectorParameterValue : CustomVectorParameterValues)
	{
		MID->SetVectorParameterValue(VectorParameterValue.ParameterInfo.Name, VectorParameterValue.ParameterValue);
	}
}
