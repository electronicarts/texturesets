// (c) Electronic Arts. All Rights Reserved.

#include "STextureSetParameterWidget.h"

#include "Containers/BitArray.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Children.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "SlotBase.h"
#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateTypes.h"
#include "Templates/ChooseClass.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "PropertyCustomizationHelpers.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetAssetUserData.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "TextureSetEditingUtils.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"

#define LOCTEXT_NAMESPACE "FTextureSetsModule"

void STextureSetParameterWidget::Construct(const FArguments& InArgs, UMaterialInstance* InMaterialInstance, UINT InParameterIndex)
{
	MaterialInstance = InMaterialInstance;
	ParameterIndex = InParameterIndex;

	UTextureSetAssetUserData* TextureSetOverrides = MaterialInstance->GetAssetUserData<UTextureSetAssetUserData>();
	const FSetOverride & SetOverride = TextureSetOverrides->TexturesSetOverrides[ParameterIndex];

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(FMargin(0, 1, 0, 1))
		.FillWidth(1.0f)
		[
			SNew(SCheckBox)
			.ToolTipText(LOCTEXT("OverrideTextureSetParameter", "Toggle whether or not to override this texture set shader parameter."))
			.Type(ESlateCheckBoxType::CheckBox)
			.IsChecked(this, &STextureSetParameterWidget::IsTextureSetOverridden)
			.OnCheckStateChanged(this, &STextureSetParameterWidget::ToggleTextureSetOverridden)
			.Padding(4)
			[
				SNew(STextBlock)
				.Text(FText::FromName(SetOverride.Name))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(FMargin(0, 1, 0, 1))
		.FillWidth(1.0f)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(UTextureSet::StaticClass())
			.ObjectPath(this, &STextureSetParameterWidget::GetTextureSetAssetPath)
			.OnObjectChanged(this, &STextureSetParameterWidget::OnTextureSetAssetChanged)
			.OnShouldFilterAsset(this, &STextureSetParameterWidget::OnShouldFilterTextureSetAsset)
		]
	];

}

ECheckBoxState STextureSetParameterWidget::IsTextureSetOverridden() const
{
	if (MaterialInstance)
	{
		UTextureSetAssetUserData* TextureSetOverrides = MaterialInstance->GetAssetUserData<UTextureSetAssetUserData>();
		if (TextureSetOverrides)
		{
			if (TextureSetOverrides->TexturesSetOverrides.IsValidIndex(ParameterIndex))
			{
				const FSetOverride& TextureSetOverride = TextureSetOverrides->TexturesSetOverrides[ParameterIndex];
				return TextureSetOverride.IsOverridden ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}
		}
	}

	return ECheckBoxState::Unchecked;
}

void STextureSetParameterWidget::ToggleTextureSetOverridden(ECheckBoxState NewState)
{
	if (MaterialInstance)
	{
		UTextureSetAssetUserData* TextureSetOverrides = MaterialInstance->GetAssetUserData<UTextureSetAssetUserData>();
		if (TextureSetOverrides)
		{
			if (TextureSetOverrides->TexturesSetOverrides.IsValidIndex(ParameterIndex))
			{
				FSetOverride& TextureSetOverride = TextureSetOverrides->TexturesSetOverrides[ParameterIndex];
				TextureSetOverride.IsOverridden = NewState == ECheckBoxState::Checked ? true : false;

				FTextureSetEditingUtils::UpdateMaterialInstance(MaterialInstance);

				FPropertyChangedEvent DymmyEvent(nullptr);
				MaterialInstance->PostEditChangeProperty(DymmyEvent);
			}
		}
	}

}

bool STextureSetParameterWidget::OnShouldFilterTextureSetAsset(const FAssetData& InAssetData)
{
	UTextureSet* NewTextureSet = (UTextureSet*)InAssetData.GetAsset();

	if (NewTextureSet)
	{
		if (MaterialInstance)
		{
			UTextureSetAssetUserData* TextureSetOverrides = MaterialInstance->GetAssetUserData<UTextureSetAssetUserData>();
			if (IsValid(TextureSetOverrides))
			{
				if (TextureSetOverrides->TexturesSetOverrides.IsValidIndex(ParameterIndex))
				{
					FSetOverride& TextureSetOverride = TextureSetOverrides->TexturesSetOverrides[ParameterIndex];

					const FString& currentDefinitionPath = TextureSetOverride.Definition->GetPathName();
					const FString& newDefinitionPath = NewTextureSet->Definition->GetPathName();
					if (newDefinitionPath == currentDefinitionPath)
						return false;
				}
			}
		}
	}
	return true;
}

FString STextureSetParameterWidget::GetTextureSetAssetPath() const
{
	if (MaterialInstance)
	{
		UTextureSetAssetUserData* TextureSetOverrides = MaterialInstance->GetAssetUserData<UTextureSetAssetUserData>();
		if (TextureSetOverrides)
		{
			if (TextureSetOverrides->TexturesSetOverrides.IsValidIndex(ParameterIndex))
			{
				FSetOverride& TextureSetOverride = TextureSetOverrides->TexturesSetOverrides[ParameterIndex];
				
				if (TextureSetOverride.TextureSet)
					return TextureSetOverride.TextureSet->GetPathName();
				else if (TextureSetOverride.DefaultTextureSet)
					return TextureSetOverride.DefaultTextureSet->GetPathName();
			}
		}
	}

	return FString();
}

void STextureSetParameterWidget::OnTextureSetAssetChanged(const FAssetData& InAssetData)
{
	UTextureSet* NewTextureSet = (UTextureSet*)InAssetData.GetAsset();

	if (NewTextureSet)
	{
		if (MaterialInstance)
		{
			UTextureSetAssetUserData* TextureSetOverrides = MaterialInstance->GetAssetUserData<UTextureSetAssetUserData>();
			if (TextureSetOverrides)
			{
				if (TextureSetOverrides->TexturesSetOverrides.IsValidIndex(ParameterIndex))
				{
					FSetOverride& TextureSetOverride = TextureSetOverrides->TexturesSetOverrides[ParameterIndex];

					TextureSetOverride.TextureSet = NewTextureSet;			

					FTextureSetEditingUtils::UpdateMaterialInstance(MaterialInstance);

					FPropertyChangedEvent DymmyEvent(nullptr);
					MaterialInstance->PostEditChangeProperty(DymmyEvent);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
