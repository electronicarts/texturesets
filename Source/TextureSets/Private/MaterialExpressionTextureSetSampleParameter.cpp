// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "MaterialExpressionTextureSetSampleParameter.h"

#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetModule.h"
#include "TextureSetsHelpers.h"
#include "UObject/ObjectSaveContext.h"
#if WITH_EDITOR
#include "TextureSetSampleFunctionBuilder.h"
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

UMaterialExpressionTextureSetSampleParameter::UMaterialExpressionTextureSetSampleParameter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ParameterName = "Texture Set";

	bShowOutputNameOnPin = true;
	bShowOutputs = true;

	bIsParameterExpression = true;

	// Update parameter on construction if it's invalid
	UpdateParameterGuid(false, true);

#if WITH_EDITOR
	OnTextureSetDefinitionChangedHandle = UTextureSetDefinition::FOnTextureSetDefinitionChangedEvent.AddUObject(this, &UMaterialExpressionTextureSetSampleParameter::OnDefinitionChanged);
#endif
}

#if WITH_EDITOR
bool UMaterialExpressionTextureSetSampleParameter::ConfigureMaterialFunction(class UMaterialFunction* NewMaterialFunction)
{
	// Clear builder errors, because we don't want previous build errors preventing us from rebuilding
	BuilderErrors.Empty();

	FDataValidationContext ValidationContext;
	if (IsDataValid(ValidationContext) == EDataValidationResult::Invalid)
		return false;

	// Make sure our samping parameters are up to date before generating the sampling graph
	UpdateSampleParamArray();

	FTextureSetMaterialGraphBuilderArgs Args;
	Args.ParameterName = ParameterName;
	Args.ParameterGroup = Group;
	Args.MaterialFunction = NewMaterialFunction;
	Args.ModuleInfo = Definition->GetModuleInfo();
	Args.PackingInfo = Definition->GetPackingInfo();
	Args.DefaultDerivedData = Definition->GetDefaultTextureSet()->GetDerivedData();
	Args.SampleParams = SampleParams;
	Args.SampleContext = { BaseNormalSource, TangentSource, PositionSource, CameraVectorSource };

	const FTextureSetSampleFunctionBuilder Builder(Args);

	BuilderErrors = Builder.GetErrors();

	UMaterialEditingLibrary::LayoutMaterialFunctionExpressions(NewMaterialFunction);

	return BuilderErrors.IsEmpty();
}
#endif

FGuid& UMaterialExpressionTextureSetSampleParameter::GetParameterExpressionId()
{
	return ExpressionGUID;
}

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
	if (!IsValid(DefaultTextureSet.Get()) && IsValid(Definition))
	{
		OutMeta.Value = Definition->GetDefaultTextureSet();
	}
	
	OutMeta.Description = Desc;
	OutMeta.ExpressionGuid = ExpressionGUID;
	OutMeta.Group = Group;
	OutMeta.SortPriority = SortPriority;
	OutMeta.AssetPath = GetAssetPathName();
	// Parameter class will be null when executable is compiled with editor, but run standalone, as the TextureSetsEditor module will not be loaded.
	OutMeta.CustomParameterClass = FindObject<UClass>(FTopLevelAssetPath("/Script/TextureSetsEditor.DEditorTextureSetParameterValue"));
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

#if WITH_EDITOR
void UMaterialExpressionTextureSetSampleParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(ParameterName.ToString());
}
#endif

void UMaterialExpressionTextureSetSampleParameter::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	UTextureSetDefinition::FOnTextureSetDefinitionChangedEvent.Remove(OnTextureSetDefinitionChangedHandle);
#endif
}

void UMaterialExpressionTextureSetSampleParameter::PreSave(FObjectPreSaveContext SaveContext)
{
	if (!IsValid(DefaultTextureSet.Get()) && SaveContext.IsCooking())
    {
    	ensure (IsValid(Definition));
    	DefaultTextureSet = const_cast<UTextureSet*>(Definition->GetDefaultTextureSet());
    }
	Super::PreSave(SaveContext);
}

#if WITH_EDITOR
EDataValidationResult UMaterialExpressionTextureSetSampleParameter::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	// Validate parameter name
	if (ParameterName.IsNone())
	{
		Context.AddError(LOCTEXT("BadParamName","Texture Set Parameter name cannot be 'None', please specify a valid name."));
		Result = EDataValidationResult::Invalid;
	}

	// Validate definition
	if (!IsValid(Definition))
	{
		Context.AddError(LOCTEXT("MissingDefinition","A texture set sample must reference a valid definition."));
		Result = EDataValidationResult::Invalid;
	}
	else if (Definition->IsDataValid(Context) == EDataValidationResult::Invalid)
	{
		Result = EDataValidationResult::Invalid;
	}

	// Validate default texture set
	if (DefaultTextureSet.IsValid() && DefaultTextureSet->Definition != Definition)
	{
		Context.AddError(LOCTEXT("MismatchedDefinition","The Default texture set does not use the same definition as specified in the material expression parameter."));
		Result = EDataValidationResult::Invalid;
	}

	// Validate builder errors
	if (!BuilderErrors.IsEmpty())
	{
		for (FText Error : BuilderErrors)
			Context.AddError(Error);

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

	TArray<TSubclassOf<UTextureSetAssetParams>> RequiredSampleParamClasses(Definition->GetRequiredSampleParamClasses());

	SampleParams.UpdateParamList(this, RequiredSampleParamClasses);
}
#endif

#if WITH_EDITOR
void UMaterialExpressionTextureSetSampleParameter::OnDefinitionChanged(UTextureSetDefinition* ChangedDefinition)
{
	// Only update if this is our definition, or if we're not going to call post-load later (that will update everything anyway).
	if (ChangedDefinition == Definition && !HasAnyFlags(RF_NeedPostLoad))
	{
		// Regenerates the material function
		UpdateMaterialFunction();

		if (GIsEditor && !IsLoading())
		{
			// Notifies the editor the function has changed and things need to be recompiled/redrawn
			UMaterialEditingLibrary::UpdateMaterialFunction(MaterialFunction);
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
