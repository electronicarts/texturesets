// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FName;
class FString;
class UMaterialInstance;

/**
 * The widget to display metadata as a table of tag/value rows
 */
class STextureSetParameterWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STextureSetParameterWidget)	{}
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	InMetaData			The metadata tags/values to display in the table view widget
	 */
	void Construct(const FArguments& InArgs, UMaterialInstance* MaterialInstance, UINT ParameterIndex);

private:

	ECheckBoxState IsTextureSetOverridden() const;
	void ToggleTextureSetOverridden(ECheckBoxState NewState);

	bool OnShouldFilterTextureSetAsset(const FAssetData& AssetData);
	FString GetTextureSetAssetPath() const;
	void OnTextureSetAssetChanged(const FAssetData& InAssetData);

	UMaterialInstance* MaterialInstance;
	UINT ParameterIndex;
};