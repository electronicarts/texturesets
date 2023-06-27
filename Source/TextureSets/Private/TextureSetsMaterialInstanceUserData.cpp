// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetsMaterialInstanceUserData.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/ObjectSaveContext.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "TextureSetDefinition.h"
#include "TextureSet.h"

#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#endif

#if WITH_EDITOR
const UMaterialExpressionTextureSetSampleParameter* UTextureSetsMaterialInstanceUserData::FindSampleExpression(const FGuid& NodeID, UMaterial* Material)
{
	// FindExpressionByGUID() doesn't work because it ignores subclasses of material function calls. We need to re-implement a search.

	TArray<const UMaterialExpressionTextureSetSampleParameter*> SamplerExpressions;
	Material->GetAllExpressionsOfType<UMaterialExpressionTextureSetSampleParameter>(SamplerExpressions);

	for (const UMaterialExpressionTextureSetSampleParameter* CurNode : SamplerExpressions)
	{
		if (CurNode->MaterialExpressionGuid == NodeID)
			return CurNode;
	}
	return nullptr;
}
#endif

#if WITH_EDITOR
void UTextureSetsMaterialInstanceUserData::UpdateAssetUserData(UMaterialInstance* MaterialInstance)
{
	check(MaterialInstance);

	if (!IsValid(MaterialInstance->GetMaterial()))
		return; // Possible if parent has not been assigned yet.

	TArray<const UMaterialExpressionTextureSetSampleParameter*> TextureSetExpressions;
	MaterialInstance->GetMaterial()->GetAllExpressionsOfType<UMaterialExpressionTextureSetSampleParameter>(TextureSetExpressions);

	UTextureSetsMaterialInstanceUserData* TextureSetUserData = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();

	// If there are no texture set expressions, we can clear TextureSetUserData
	if (TextureSetExpressions.IsEmpty() && IsValid(TextureSetUserData))
	{
		MaterialInstance->RemoveUserDataOfClass(UTextureSetsMaterialInstanceUserData::StaticClass());
		return; // Done here, not a material that needs texture set stuff.
	}

	TArray<FGuid> UnusedOverrides;

	if (IsValid(TextureSetUserData))
		TextureSetUserData->TexturesSetOverrides.GetKeys(UnusedOverrides);

	for (const UMaterialExpressionTextureSetSampleParameter* TextureSetExpression : TextureSetExpressions)
	{
		check(TextureSetExpression);

		// Add texture set user data if it doesn't exist
		// Note that this happens inside the loop of texture set expressions.
		// This is so the user data is only added if there is a texture set expression.
		if (!IsValid(TextureSetUserData))
		{
			TextureSetUserData = NewObject<UTextureSetsMaterialInstanceUserData>(MaterialInstance);
			MaterialInstance->AddAssetUserData(TextureSetUserData);
		}

		const FGuid NodeGuid = TextureSetExpression->MaterialExpressionGuid;
		if (TextureSetUserData->TexturesSetOverrides.Contains(NodeGuid))
		{
			UnusedOverrides.RemoveSingle(NodeGuid);

			// Update existing override with the correct value
			FSetOverride Override = TextureSetUserData->GetOverride(NodeGuid);
			if (Override.Name != TextureSetExpression->ParameterName)
			{
				Override.Name = TextureSetExpression->ParameterName;
				TextureSetUserData->SetOverride(NodeGuid, Override);
			}
		}
		else
		{
			// Add new override
			FSetOverride Override;

			// TODO: See if there are any old overrides with the same name, which we could use to recover values from.
			Override.Name = TextureSetExpression->ParameterName;
			Override.TextureSet = TextureSetExpression->DefaultTextureSet;
			Override.IsOverridden = false;

			TextureSetUserData->TexturesSetOverrides.Add(NodeGuid, Override);
		}
	}

	// Clear any overrides we no longer need.
	for (FGuid Unused : UnusedOverrides)
		TextureSetUserData->TexturesSetOverrides.Remove(Unused);
}
#endif

void UTextureSetsMaterialInstanceUserData::PostInitProperties()
{
	Super::PostInitProperties();
	if (GetClass()->GetDefaultObject() != this)
	{
		MaterialInstance = Cast<UMaterialInstance>(GetOuter());
		checkf(MaterialInstance, TEXT("Texture set user data must be created with a UMaterialInstance as the outer."))
	}
}

void UTextureSetsMaterialInstanceUserData::PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext)
{
	Super::PreSaveRoot(ObjectSaveContext);

	// Clear any texture set parameter from the material instance
	// We want to make sure they're only set at runtime and not serialized.
	ClearTextureSetParameters();
}

void UTextureSetsMaterialInstanceUserData::PostLoadOwner()
{
	Super::PostLoadOwner();
#if WITH_EDITOR
	// Update the texture set parameters when the MaterialInstance has finished loading.
	UpdateTextureSetParameters();
#endif
}

void UTextureSetsMaterialInstanceUserData::ClearTextureSetParameters()
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

#if WITH_EDITOR
void UTextureSetsMaterialInstanceUserData::UpdateTextureSetParameters()
{
	if (!IsValid(MaterialInstance->GetMaterial()))
		return;
	
	// Remove any exsting texture set related parameters, as we're about to re-create the ones that are needed.
	ClearTextureSetParameters();

	for (const auto& [Guid, TextureSetOverride] : TexturesSetOverrides)
	{
		if (!TextureSetOverride.IsOverridden || TextureSetOverride.TextureSet == nullptr)
			continue;

		const UMaterialExpressionTextureSetSampleParameter* SampleExpression = FindSampleExpression(Guid, MaterialInstance->GetMaterial());
		if (SampleExpression == nullptr)
			continue;

		UTextureSetDefinition* Definition = SampleExpression->Definition;
		if (Definition == nullptr)
			continue;

		const UTextureSetDerivedData* DerivedData = TextureSetOverride.TextureSet->GetDerivedData();

		// Set any constant parameters what we have
		for (auto& [Name, Value] : DerivedData->MaterialParameters)
		{
			FVectorParameterValue Parameter;
			Parameter.ParameterValue = FLinearColor(Value);
			Parameter.ParameterInfo.Name = SampleExpression->GetConstantParameterName(Name);
			MaterialInstance->VectorParameterValues.Add(Parameter);
		}

		for (int i = 0; i < DerivedData->PackedTextureData.Num(); i++)
		{
			const FPackedTextureData& PackedTextureData = DerivedData->PackedTextureData[i];

			// Set the texture parameter for each packed texture
			FTextureParameterValue TextureParameter;
			TextureParameter.ParameterValue = TextureSetOverride.IsOverridden ? TextureSetOverride.TextureSet->GetDerivedTexture(i) : nullptr;
			TextureParameter.ParameterInfo.Name = SampleExpression->GetTextureParameterName(i);
			MaterialInstance->TextureParameterValues.Add(TextureParameter);

			// Set any constant parameters that come with this texture
			for (auto& [Name, Value] : PackedTextureData.MaterialParameters)
			{
				FVectorParameterValue Parameter;
				Parameter.ParameterValue = FLinearColor(Value);
				Parameter.ParameterInfo.Name = SampleExpression->GetConstantParameterName(Name);
				MaterialInstance->VectorParameterValues.Add(Parameter);
			}
		}
	}
}
#endif

const TArray<FGuid> UTextureSetsMaterialInstanceUserData::GetOverrides() const
{
	TArray<FGuid> Guids;
	TexturesSetOverrides.GetKeys(Guids);
	return Guids;
}

const FSetOverride& UTextureSetsMaterialInstanceUserData::GetOverride(FGuid Guid) const
{
	return TexturesSetOverrides.FindChecked(Guid);
}

void UTextureSetsMaterialInstanceUserData::SetOverride(FGuid Guid, const FSetOverride& Override)
{
	TexturesSetOverrides.Add(Guid, Override);
#if WITH_EDITOR
	UpdateTextureSetParameters();

	FPropertyChangedEvent DummyEvent(nullptr);
	MaterialInstance->PostEditChangeProperty(DummyEvent);

	// Trigger a redraw of the UI
	UMaterialEditingLibrary::RefreshMaterialEditor(MaterialInstance);
#endif
}
