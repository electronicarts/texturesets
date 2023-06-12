// (c) Electronic Arts. All Rights Reserved.

#if WITH_EDITOR
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
#include "TextureSetsMaterialInstanceUserData.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "PropertyCustomizationHelpers.h"
#include "MaterialExpressionTextureSetSampleParameter.h"

#define LOCTEXT_NAMESPACE "FTextureSetsModule"

void STextureSetParameterWidget::Construct(const FArguments& InArgs, UMaterialInstanceConstant* InMaterialInstance, FGuid InParameter)
{
	check(InMaterialInstance);
	MaterialInstance = InMaterialInstance;
	Parameter = InParameter;

	UTextureSetsMaterialInstanceUserData* UserData = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();
	check(UserData);

	const FSetOverride& Override = UserData->GetOverride(Parameter);

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
				.Text(FText::FromName(Override.Name))
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
	UTextureSetsMaterialInstanceUserData* UserData = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();
	check(UserData);

	const FSetOverride& Override = UserData->GetOverride(Parameter);
	return Override.IsOverridden ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

	return ECheckBoxState::Unchecked;
}

void STextureSetParameterWidget::ToggleTextureSetOverridden(ECheckBoxState NewState)
{
	UTextureSetsMaterialInstanceUserData* UserData = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();
	check(UserData);

	FSetOverride Override = UserData->GetOverride(Parameter);
	Override.IsOverridden = (NewState == ECheckBoxState::Checked);

	UserData->SetOverride(Parameter, Override);
}

bool STextureSetParameterWidget::OnShouldFilterTextureSetAsset(const FAssetData& InAssetData)
{
	check(MaterialInstance->GetMaterial());

	const UTextureSet* NewTextureSet = CastChecked<UTextureSet>(InAssetData.GetAsset());
	check(NewTextureSet);

	const UMaterialExpressionTextureSetSampleParameter* SampleExpression = UTextureSetsMaterialInstanceUserData::FindSampleExpression(Parameter, MaterialInstance->GetMaterial());
	check(SampleExpression);

	return NewTextureSet->Definition != SampleExpression->Definition;
}

FString STextureSetParameterWidget::GetTextureSetAssetPath() const
{
	check(MaterialInstance);

	const UTextureSetsMaterialInstanceUserData* UserData = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();
	check(UserData);

	const FSetOverride& TextureSetOverride = UserData->GetOverride(Parameter);
				
	if (TextureSetOverride.TextureSet)
		return TextureSetOverride.TextureSet->GetPathName();
	else
		return FString();
}

void STextureSetParameterWidget::OnTextureSetAssetChanged(const FAssetData& InAssetData)
{
	check(MaterialInstance);

	UTextureSetsMaterialInstanceUserData* UserData = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();
	check(UserData);

	FSetOverride TextureSetOverride = UserData->GetOverride(Parameter);
	TextureSetOverride.TextureSet = Cast<UTextureSet>(InAssetData.GetAsset());;

	UserData->SetOverride(Parameter, TextureSetOverride);
}

#undef LOCTEXT_NAMESPACE
#endif