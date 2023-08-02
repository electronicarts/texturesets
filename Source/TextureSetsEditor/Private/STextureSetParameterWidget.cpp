// (c) Electronic Arts. All Rights Reserved.

#include "STextureSetParameterWidget.h"

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

void STextureSetParameterWidget::Construct(const FArguments& InArgs, UMaterialInstanceConstant* InMaterialInstance, FMaterialParameterInfo InParameter, const UMaterialExpressionTextureSetSampleParameter* InExpression)
{
	check(InMaterialInstance);
	MaterialInstance = InMaterialInstance;
	Parameter = InParameter;
	Expression = InExpression;

	UTextureSetsMaterialInstanceUserData* UserData = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();
	check(UserData);

	FString ParameterTitle = InParameter.Name.ToString();

	// TODO: When these are properly shown in the layers, we won't need to specify where they're from
	if (InParameter.Association == EMaterialParameterAssociation::BlendParameter)
		ParameterTitle += " Blend " + FString::FromInt(InParameter.Index);
	else if (InParameter.Association == EMaterialParameterAssociation::LayerParameter)
		ParameterTitle += " Layer " + FString::FromInt(InParameter.Index);

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
				.Text(FText::FromString(ParameterTitle))
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

	FTextureSetOverride Override;
	if (UserData->GetOverride(Parameter, Override))
		return Override.IsOverridden ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

	return ECheckBoxState::Unchecked;
}

void STextureSetParameterWidget::ToggleTextureSetOverridden(ECheckBoxState NewState)
{
	UTextureSetsMaterialInstanceUserData* UserData = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();
	check(UserData);

	FTextureSetOverride Override;
	if (UserData->GetOverride(Parameter, Override))
	{
		Override.IsOverridden = (NewState == ECheckBoxState::Checked);

		UserData->SetOverride(Override);
	}
}

bool STextureSetParameterWidget::OnShouldFilterTextureSetAsset(const FAssetData& InAssetData)
{
	check(MaterialInstance->GetMaterial());

	const UTextureSet* NewTextureSet = CastChecked<UTextureSet>(InAssetData.GetAsset());
	check(NewTextureSet);

	check(Expression);

	return NewTextureSet->Definition != Expression->Definition;
}

FString STextureSetParameterWidget::GetTextureSetAssetPath() const
{
	check(MaterialInstance);

	const UTextureSetsMaterialInstanceUserData* UserData = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();
	check(UserData);

	FTextureSetOverride Override;
	if (UserData->GetOverride(Parameter, Override) && IsValid(Override.TextureSet))
		return Override.TextureSet->GetPathName();
	else
		return FString();
}

void STextureSetParameterWidget::OnTextureSetAssetChanged(const FAssetData& InAssetData)
{
	check(MaterialInstance);

	UTextureSetsMaterialInstanceUserData* UserData = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();
	check(UserData);

	FTextureSetOverride Override;
	if (UserData->GetOverride(Parameter, Override))
	{
		Override.TextureSet = Cast<UTextureSet>(InAssetData.GetAsset());

		UserData->SetOverride(Override);
	}
}

#undef LOCTEXT_NAMESPACE