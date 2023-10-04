// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetDetails.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "IPropertyUtilities.h"
#include "TextureSetAssetParams.h"

#define LOCTEXT_NAMESPACE "TextureSets"

FTextureSetDetails::~FTextureSetDetails()
{
	if (RefreshHandle.IsValid())
		FTextureSetAssetParamsCollection::OnCollectionChangedDelegate.Remove(RefreshHandle);
}

const FName FTextureSetDetails::GetPropertyTypeName()
{
	static const FName TypeName = "TextureSetAssetParamsCollection";
	return TypeName;
}

TSharedRef<IPropertyTypeCustomization> FTextureSetDetails::MakeInstance()
{
	return MakeShared<FTextureSetDetails>();
}

void FTextureSetDetails::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const bool bShowInnerPropsOnly = PropertyHandle->HasMetaData(TEXT("ShowOnlyInnerProperties"));

	if (!bShowInnerPropsOnly)
	{
		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		];
	}
}

void FTextureSetDetails::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Draw all properties of sub-objects as if they were part of this struct
	TSharedPtr<IPropertyHandle> ParamListHandle = PropertyHandle->GetChildHandle("ParamList", false);

	uint32 NumAssetParams = 0;
	ParamListHandle->GetNumChildren(NumAssetParams);
	for (uint32 AssetParamIdx = 0; AssetParamIdx < NumAssetParams; ++AssetParamIdx)
	{
		TSharedPtr<IPropertyHandle> ParamHandle = ParamListHandle->GetChildHandle(AssetParamIdx);

		uint32 NumProperties = 0;
		ParamHandle->GetChildHandle(0)->GetNumChildren(NumProperties);
		for (uint32 PropertyIdx = 0; PropertyIdx < NumProperties; ++PropertyIdx)
		{
			ChildBuilder.AddProperty(ParamHandle->GetChildHandle(0)->GetChildHandle(PropertyIdx).ToSharedRef());
		}
	}

	// Get notifications when object changes
	// TODO: There must be a more targeted way to do this
	TSharedPtr<IPropertyUtilities> PropertyUtilities = CustomizationUtils.GetPropertyUtilities();
	
	if (RefreshHandle.IsValid())
		FTextureSetAssetParamsCollection::OnCollectionChangedDelegate.Remove(RefreshHandle);

	RefreshHandle = FTextureSetAssetParamsCollection::OnCollectionChangedDelegate.AddLambda([PropertyUtilities]()
	{
		PropertyUtilities->ForceRefresh();
	});
}
#undef LOCTEXT_NAMESPACE
