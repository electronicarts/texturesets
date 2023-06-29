// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FString;
class UMaterialInstanceConstant;

/**
 * The widget to display metadata as a table of tag/value rows
 */
class STextureSetParameterWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STextureSetParameterWidget)	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UMaterialInstanceConstant* MaterialInstance, FGuid Parameter);

private:

	ECheckBoxState IsTextureSetOverridden() const;
	void ToggleTextureSetOverridden(ECheckBoxState NewState);

	bool OnShouldFilterTextureSetAsset(const FAssetData& AssetData);
	FString GetTextureSetAssetPath() const;
	void OnTextureSetAssetChanged(const FAssetData& InAssetData);

	UMaterialInstanceConstant* MaterialInstance;
	FGuid Parameter;
};
