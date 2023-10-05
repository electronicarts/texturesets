// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetSourceTextureReferenceCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "IPropertyUtilities.h"
#include "TextureSetAssetParams.h"
#include "TextureSetSourceTextureReference.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TextureSets"

const FName FTextureSetSourceTextureReferenceCustomization::GetPropertyTypeName()
{
	static const FName TypeName = "TextureSetSourceTextureReference";
	return TypeName;
}

TSharedRef<IPropertyTypeCustomization> FTextureSetSourceTextureReferenceCustomization::MakeInstance()
{
	return MakeShared<FTextureSetSourceTextureReferenceCustomization>();
}

void FTextureSetSourceTextureReferenceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	];

	HeaderRow.ValueContent()
	[
		PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTextureSetSourceTextureReference, Texture))->CreatePropertyValueWidget()
	];
}

void FTextureSetSourceTextureReferenceCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	if (PropertyHandle->IsValidHandle())
	{
		uint32 NumChildren = 0;
		PropertyHandle->GetNumChildren(NumChildren);

		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
		{
			TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();

			if (ChildHandle->GetProperty()->GetFName() != GET_MEMBER_NAME_CHECKED(FTextureSetSourceTextureReference, Texture))
			{
				ChildBuilder.AddProperty(ChildHandle);
			}
		}
	}
}
#undef LOCTEXT_NAMESPACE
