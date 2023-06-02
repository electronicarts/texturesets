// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetsMaterialInstanceUserData.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "TextureSetEditingUtils.h"
#include "UObject/ObjectSaveContext.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialPropertyHelpers.h"
#include "TextureSetDefinition.h"
#include "TextureSet.h"

#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "MaterialEditor/Public/MaterialEditorModule.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "STextureSetParameterWidget.h"

FDelegateHandle UTextureSetsMaterialInstanceUserData::OnMaterialInstanceOpenedForEditHandle;
FDelegateHandle UTextureSetsMaterialInstanceUserData::OnMICreateGroupsWidgetHandle;
#endif

void UTextureSetsMaterialInstanceUserData::RegisterCallbacks()
{
	UnregisterCallbacks();

#if WITH_EDITOR
	IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>("MaterialEditor");
	OnMaterialInstanceOpenedForEditHandle = MaterialEditorModule->OnMaterialInstanceOpenedForEdit().AddStatic(&UTextureSetsMaterialInstanceUserData::OnMaterialInstanceOpenedForEdit);
	
	OnMICreateGroupsWidgetHandle = UMaterialEditorInstanceConstant::OnCreateGroupsWidget.AddStatic(&UTextureSetsMaterialInstanceUserData::OnMICreateGroupsWidget);
#endif
}

void UTextureSetsMaterialInstanceUserData::UnregisterCallbacks()
{
#if WITH_EDITOR
	if (OnMaterialInstanceOpenedForEditHandle.IsValid())
	{
		IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>("MaterialEditor");
		MaterialEditorModule->OnMaterialInstanceOpenedForEdit().Remove(OnMaterialInstanceOpenedForEditHandle);
		OnMaterialInstanceOpenedForEditHandle.Reset();
	}

	if (OnMICreateGroupsWidgetHandle.IsValid())
	{
		UMaterialEditorInstanceConstant::OnCreateGroupsWidget.Remove(OnMICreateGroupsWidgetHandle);
		OnMICreateGroupsWidgetHandle.Reset();
	}
#endif
}

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

	// Update the texture set parameters when the MaterialInstance has finished loading.
	UpdateTextureSetParameters();
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

		const UMaterialExpressionTextureSetSampleParameter* SampleExpression = FTextureSetEditingUtils::FindSampleExpression(Guid, MaterialInstance->GetMaterial());
		if (SampleExpression == nullptr)
			continue;

		UTextureSetDefinition* Definition = SampleExpression->Definition;
		if (Definition == nullptr)
			continue;

		const int NumPackedTextures = Definition->GetPackingInfo().NumPackedTextures();

		// Set the texture parameter for each cooked texture
		for (int i = 0; i < NumPackedTextures; i++)
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

const FSetOverride& UTextureSetsMaterialInstanceUserData::GetOverride(FGuid Guid) const
{
	return TexturesSetOverrides.FindChecked(Guid);
}

void UTextureSetsMaterialInstanceUserData::SetOverride(FGuid Guid, const FSetOverride& Override)
{
	TexturesSetOverrides.Add(Guid, Override);
	UpdateTextureSetParameters();

#if WITH_EDITOR
	FPropertyChangedEvent DummyEvent(nullptr);
	MaterialInstance->PostEditChangeProperty(DummyEvent);

	// Trigger a redraw of the UI
	UMaterialEditingLibrary::RefreshMaterialEditor(MaterialInstance);
#endif
}

void UTextureSetsMaterialInstanceUserData::OnMaterialInstanceOpenedForEdit(UMaterialInstance* MaterialInstance)
{
	UpdateAssetUserData(MaterialInstance);
}

void UTextureSetsMaterialInstanceUserData::OnMICreateGroupsWidget(TObjectPtr<UMaterialInstanceConstant> MaterialInstance, IDetailCategoryBuilder& GroupsCategory)
{
	check(MaterialInstance);

	UTextureSetsMaterialInstanceUserData* TextureSetOverrides = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();
	// Not an error if user data doesn't exist, just means this material doesn't contain a texture set.
	if (!TextureSetOverrides)
		return;

	const FName& GroupName = FMaterialPropertyHelpers::TextureSetParamName;
	IDetailGroup& DetailGroup = GroupsCategory.AddGroup(GroupName, FText::FromName(GroupName), false, true);

	DetailGroup.HeaderRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromName(DetailGroup.GetGroupName()))
		];

	TArray<FGuid> Guids;
	TextureSetOverrides->TexturesSetOverrides.GetKeys(Guids);

	for (const FGuid& Guid : Guids)
	{
		DetailGroup.AddWidgetRow()
			[
				SNew(STextureSetParameterWidget, MaterialInstance, Guid)
			];
	}

}
