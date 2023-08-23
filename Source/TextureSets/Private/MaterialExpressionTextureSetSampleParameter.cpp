// (c) Electronic Arts. All Rights Reserved.

#include "MaterialExpressionTextureSetSampleParameter.h"

#include "TextureSetMaterialGraphBuilder.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "HLSLTree/HLSLTreeCommon.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "IDetailPropertyRow.h"
#include "MaterialPropertyHelpers.h"
#include "Widgets/SBoxPanel.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

UDEditorTextureSetParameterValue::UDEditorTextureSetParameterValue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ParameterValue = nullptr;
}

bool UDEditorTextureSetParameterValue::GetValue(FMaterialParameterMetadata& OutResult) const
{
	UDEditorCustomParameterValue::GetValue(OutResult);
	OutResult.Value = FMaterialParameterValue((UObject*)ParameterValue.Get());
	OutResult.CustomParameterClass = UDEditorTextureSetParameterValue::StaticClass();
	return true;
}

bool UDEditorTextureSetParameterValue::SetValue(const FMaterialParameterValue& Value)
{
	if (Value.Type == EMaterialParameterType::Custom)
	{
		// Null values are valid, objects that fail to cast to a texture set are not
		UTextureSet* TextureSetValue = Cast<UTextureSet>(Value.AsCustomObject());
		if (TextureSetValue != nullptr || Value.AsCustomObject() == nullptr)
		{
			ParameterValue = TextureSetValue;
			return true;
		}
	}
	return false;
}

//void UDEditorTextureSetParameterValue::CreateWidget(IDetailPropertyRow& Row, FText NameOverride, FSortedParamData* StackParameterData, UMaterialEditorInstanceConstant* MaterialEditorInstance)
//{
//	// TODO
//	//FDetailWidgetDecl* CustomNameWidget = Row.CustomNameWidget();
//	//if (CustomNameWidget)
//	//{
//	//	FDetailWidgetDecl* CustomNameWidget = Row.CustomNameWidget();
//	//	if (CustomNameWidget)
//	//	{
//	//		(*CustomNameWidget)
//	//		[
//	//			SNew(SHorizontalBox)
//	//			+SHorizontalBox::Slot()
//	//			.VAlign(VAlign_Center)
//	//			[
//	//				SNew(STextBlock)
//	//				.Text(NameOverride)
//	//			.ToolTipText(FMaterialPropertyHelpers::GetParameterExpressionDescription(StackParameterData->Parameter, MaterialEditorInstance))
//	//			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
//	//			]
//	//		];
//	//	}
//	//}
//}

UMaterialExpressionTextureSetSampleParameter::UMaterialExpressionTextureSetSampleParameter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bShowOutputNameOnPin = true;
	bShowOutputs         = true;

	bIsParameterExpression = true;

	// Update parameter on construction if it's invalid
	UpdateParameterGuid(false, true);

#if WITH_EDITOR
	OnTextureSetDefinitionChangedHandle = UTextureSetDefinition::FOnTextureSetDefinitionChangedEvent.AddUObject(this, &UMaterialExpressionTextureSetSampleParameter::OnDefinitionChanged);
#endif
}

FName UMaterialExpressionTextureSetSampleParameter::GetTextureParameterName(int TextureIndex) const
{
	return MakeTextureParameterName(ParameterName, TextureIndex);
}

FName UMaterialExpressionTextureSetSampleParameter::GetConstantParameterName(FName ConstantName) const
{
	return MakeConstantParameterName(ParameterName, ConstantName);
}

FName UMaterialExpressionTextureSetSampleParameter::MakeTextureParameterName(FName ParameterName, int TextureIndex)
{
	return FName("TEXSET_" + ParameterName.ToString() + "_PACKED_" + FString::FromInt(TextureIndex));
}

FName UMaterialExpressionTextureSetSampleParameter::MakeConstantParameterName(FName ParameterName, FName ConstantName)
{
	return FName("TEXSET_" + ParameterName.ToString() + "_" + ConstantName.ToString());
}

bool UMaterialExpressionTextureSetSampleParameter::IsTextureSetParameterName(FName Name)
{
	return Name.ToString().StartsWith("TEXSET_", ESearchCase::IgnoreCase);
}

#if WITH_EDITOR
UMaterialFunction* UMaterialExpressionTextureSetSampleParameter::CreateMaterialFunction()
{
	if (!IsValid(Definition))
		return nullptr;

	// Make sure our samping parameters are up to date before generating the sampling graph
	UpdateSampleParamArray();

	FTextureSetMaterialGraphBuilder GraphBuilder = FTextureSetMaterialGraphBuilder(this);

	return GraphBuilder.GetMaterialFunction();
}
#endif

#if WITH_EDITOR
FGuid& UMaterialExpressionTextureSetSampleParameter::GetParameterExpressionId()
{
	return ExpressionGUID;
}
#endif

#if WITH_EDITOR
FName UMaterialExpressionTextureSetSampleParameter::GetParameterName() const
{
	return ParameterName;
}
#endif

#if WITH_EDITOR
void UMaterialExpressionTextureSetSampleParameter::SetParameterName(const FName& Name)
{
	ParameterName = Name;
}
#endif

#if WITH_EDITOR
bool UMaterialExpressionTextureSetSampleParameter::GetParameterValue(FMaterialParameterMetadata& OutMeta) const
{
	OutMeta.Value = DefaultTextureSet.Get();
	OutMeta.Description = Desc;
	OutMeta.ExpressionGuid = ExpressionGUID;
	OutMeta.Group = Group;
	OutMeta.SortPriority = SortPriority;
	OutMeta.AssetPath = GetAssetPathName();
	OutMeta.CustomParameterClass = UDEditorTextureSetParameterValue::StaticClass();
	return true;
}
#endif

#if WITH_EDITOR
bool UMaterialExpressionTextureSetSampleParameter::SetParameterValue(const FName& Name, const FMaterialParameterMetadata& Meta, EMaterialExpressionSetParameterValueFlags Flags)
{
	if (Meta.Value.Type == EMaterialParameterType::Custom)
	{
		// Null values are valid, objects that fail to cast to a texture set are not
		UTextureSet* TextureSetValue = Cast<UTextureSet>(Meta.Value.AsCustomObject());
		if (IsValid(TextureSetValue) || Meta.Value.AsCustomObject() == nullptr)
		{
			if (Name == ParameterName)
			{
				DefaultTextureSet = TextureSetValue;

				if (EnumHasAnyFlags(Flags, EMaterialExpressionSetParameterValueFlags::SendPostEditChangeProperty))
				{
					FProperty* Property = FindFProperty<FProperty>(StaticClass(), TEXT("DefaultTextureSet"));
					FPropertyChangedEvent Event(Property);
					PostEditChangeProperty(Event);
				}

				if (EnumHasAnyFlags(Flags, EMaterialExpressionSetParameterValueFlags::AssignGroupAndSortPriority))
				{
					Group = Meta.Group;
					SortPriority = Meta.SortPriority;
				}

				return true;
			}
		}
	}
	return false;
}
#endif

void UMaterialExpressionTextureSetSampleParameter::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	UTextureSetDefinition::FOnTextureSetDefinitionChangedEvent.Remove(OnTextureSetDefinitionChangedHandle);
#endif
}

#if WITH_EDITOR
EDataValidationResult UMaterialExpressionTextureSetSampleParameter::IsDataValid(FDataValidationContext& Context)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!IsValid(Definition))
	{
		Context.AddError(LOCTEXT("MissingDefinition","A texture set sample must reference a valid definition."));
		Result = EDataValidationResult::Invalid;
	}

	return CombineDataValidationResults(Result, Super::IsDataValid(Context));
}
#endif

#if WITH_EDITOR
void UMaterialExpressionTextureSetSampleParameter::UpdateSampleParamArray()
{
	if (!IsValid(Definition))
		return;

	TArray<TSubclassOf<UTextureSetSampleParams>> RequiredSampleParamClasses = Definition->GetRequiredSampleParamClasses();

	// Remove un-needed sample params
	for (int i = 0; i < SampleParams.Num(); i++)
	{
		UClass* SampleParamClass = SampleParams[i]->GetClass();
		if (RequiredSampleParamClasses.Contains(SampleParamClass))
		{
			RequiredSampleParamClasses.Remove(SampleParamClass); // Remove this sample param from required array, so duplicates will be removed
		}
		else
		{
			SampleParams.RemoveAt(i);
			i--;
		}
	}

	// Add missing sample params
	for (TSubclassOf<UTextureSetSampleParams> SampleParamClass : RequiredSampleParamClasses)
	{
		SampleParams.Add(NewObject<UTextureSetSampleParams>(this, SampleParamClass));
	}
}
#endif

#if WITH_EDITOR
void UMaterialExpressionTextureSetSampleParameter::OnDefinitionChanged(UTextureSetDefinition* ChangedDefinition)
{
	if (ChangedDefinition == Definition)
	{
		UpdateMaterialFunction();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
