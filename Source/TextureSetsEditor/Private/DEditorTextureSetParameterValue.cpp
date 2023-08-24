// (c) Electronic Arts. All Rights Reserved.

#include "DEditorTextureSetParameterValue.h"

#include "TextureSet.h"
#include "TextureSetDefinition.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "IDetailPropertyRow.h"
#include "MaterialPropertyHelpers.h"
#include "Widgets/SBoxPanel.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

UDEditorTextureSetParameterValue::UDEditorTextureSetParameterValue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ParameterValue = nullptr;
}

bool UDEditorTextureSetParameterValue::GetValue(FMaterialParameterMetadata& OutResult) const
{
	UDEditorCustomParameterValue::GetValue(OutResult);
	OutResult.Value = FMaterialParameterValue((UObject*)ParameterValue.Get());
	OutResult.CustomParameterClass = UDEditorTextureSetParameterValue::StaticClass();
	return true;
}

bool UDEditorTextureSetParameterValue::SetValue(const FMaterialParameterValue& Value)
{
	if (Value.Type == EMaterialParameterType::Custom)
	{
		// Null values are valid, objects that fail to cast to a texture set are not
		UTextureSet* TextureSetValue = Cast<UTextureSet>(Value.AsCustomObject());
		if (TextureSetValue != nullptr || Value.AsCustomObject() == nullptr)
		{
			ParameterValue = TextureSetValue;
			return true;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
