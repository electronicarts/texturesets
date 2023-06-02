// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetAssetUserData.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "TextureSetEditingUtils.h"
#include "UObject/ObjectSaveContext.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "TextureSetDefinition.h"
#include "TextureSet.h"

void UTextureSetAssetUserData::PostInitProperties()
{
	Super::PostInitProperties();
	if (GetClass()->GetDefaultObject() != this)
	{
		MaterialInstance = Cast<UMaterialInstance>(GetOuter());
		checkf(MaterialInstance, TEXT("Texture set user data must be created with a UMaterialInstance as the outer."))
	}
}

void UTextureSetAssetUserData::PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext)
{
	Super::PreSaveRoot(ObjectSaveContext);

	// Clear any texture set parameter from the material instance
	// We want to make sure they're only set at runtime and not serialized.
	ClearTextureSetParameters();
}

void UTextureSetAssetUserData::PostLoadOwner()
{
	Super::PostLoadOwner();

	// Update the texture set parameters when the MaterialInstance has finished loading.
	UpdateTextureSetParameters();
}

void UTextureSetAssetUserData::ClearTextureSetParameters()
{
	for(int i = 0; i < MaterialInstance->TextureParameterValues.Num(); i++)
	{
		const FTextureParameterValue& TextureParameter = MaterialInstance->TextureParameterValues[i];
		if (UMaterialExpressionTextureSetSampleParameter::IsTextureSetParameterName(TextureParameter.ParameterInfo.Name))
		{
			MaterialInstance->TextureParameterValues.RemoveAt(i);
			i--;
		}
	}
}

void UTextureSetAssetUserData::UpdateTextureSetParameters()
{
	if (!IsValid(MaterialInstance->GetMaterial()))
		return;
	
	// Remove any exsting texture set related parameters, as we're about to re-create the ones that are needed.
	ClearTextureSetParameters();

	for (const auto& [Guid, TextureSetOverride] : TexturesSetOverrides)
	{
		if (!TextureSetOverride.IsOverridden || TextureSetOverride.TextureSet == nullptr)
			continue;

		const UMaterialExpressionTextureSetSampleParameter* SampleExpression = FTextureSetEditingUtils::FindSampleExpression(Guid, MaterialInstance->GetMaterial());
		if (SampleExpression == nullptr)
			continue;

		UTextureSetDefinition* Definition = SampleExpression->Definition;
		if (Definition == nullptr)
			continue;

		// Set the texture parameter for each cooked texture
		for (int i = 0; i < Definition->PackedTextures.Num(); i++)
		{
			if (TextureSetOverride.TextureSet->CookedTextures.Num() > i && TextureSetOverride.TextureSet->CookedTextures[i] != nullptr)
			{
				FTextureParameterValue TextureParameter;
				TextureParameter.ParameterValue = TextureSetOverride.IsOverridden ? TextureSetOverride.TextureSet->CookedTextures[i] : nullptr;
				TextureParameter.ParameterInfo.Name = SampleExpression->GetTextureParameterName(i);
				MaterialInstance->TextureParameterValues.Add(TextureParameter);
			}
		}
	}
}
