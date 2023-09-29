// (c) Electronic Arts. All Rights Reserved.

#include "TextureSet.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "TextureSetCooker.h"
#include "TextureSetDefinition.h"
#include "TextureSetModule.h"
#include "UObject/ObjectSaveContext.h"
#if WITH_EDITOR
#include "DerivedDataBuildVersion.h"
#include "Misc/DataValidation.h"
#include "TextureSetCompilingManager.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

UTextureSet::UTextureSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
#if WITH_EDITOR
	bIsDerivedDataReady = false;

	OnTextureSetDefinitionChangedHandle = UTextureSetDefinition::FOnTextureSetDefinitionChangedEvent.AddUObject(this, &UTextureSet::OnDefinitionChanged);
#endif
}

#if WITH_EDITOR
bool UTextureSet::IsCompiling() const
{
	if (ActiveCooker.IsValid() && !bIsDerivedDataReady)
		return true;

	for (UTexture* DerivedTexture : DerivedTextures)
	{
		// Texture set is not compiled until the derived texturess are finished compiling
		if (DerivedTexture->IsCompiling())
			return true;
	}

	return false;
}
#endif

void UTextureSet::AugmentMaterialParameters(const FCustomParameterValue& CustomParameter, TArray<FVectorParameterValue>& VectorParameters, TArray<FTextureParameterValue>& TextureParameters) const
{
	if (!IsValid(Definition))
		return;

	const UTextureSet* TS = this;

#if WITH_EDITOR
	if (!bIsDerivedDataReady)
	{
		// Use the default texture set if it's valid
		if (IsValid(Definition->GetDefaultTextureSet()) && Definition->GetDefaultTextureSet() != this)
		{
			Definition->GetDefaultTextureSet()->AugmentMaterialParameters(CustomParameter, VectorParameters, TextureParameters);
		}

		return;
	}
#endif

	// Set any constant parameters what we have
	for (const FDerivedParameterData& ParamData : TS->DerivedData->MaterialParameters)
	{
		FVectorParameterValue Parameter;
		Parameter.ParameterValue = FLinearColor(ParamData.Value);
		Parameter.ParameterInfo.Name = UMaterialExpressionTextureSetSampleParameter::MakeConstantParameterName(CustomParameter.ParameterInfo.Name, ParamData.Name);
		Parameter.ParameterInfo.Association = CustomParameter.ParameterInfo.Association;
		Parameter.ParameterInfo.Index = CustomParameter.ParameterInfo.Index;
		VectorParameters.Add(Parameter);
	}

	for (int i = 0; i < TS->DerivedData->DerivedTextureData.Num(); i++)
	{
		const FDerivedTextureData& DerivedTextureData = TS->DerivedData->DerivedTextureData[i];

		// Set the texture parameter for each packed texture
		FTextureParameterValue TextureParameter;
		TextureParameter.ParameterValue = TS->DerivedTextures[i].Get();
		TextureParameter.ParameterInfo.Name = UMaterialExpressionTextureSetSampleParameter::MakeTextureParameterName(CustomParameter.ParameterInfo.Name, i);
		TextureParameter.ParameterInfo.Association = CustomParameter.ParameterInfo.Association;
		TextureParameter.ParameterInfo.Index = CustomParameter.ParameterInfo.Index;
		TextureParameters.Add(TextureParameter);

		// Set any constant parameters that come with this texture
		for (auto& [ParameterName, Value] : DerivedTextureData.MaterialParameters)
		{
			FVectorParameterValue Parameter;
			Parameter.ParameterValue = FLinearColor(Value);
			Parameter.ParameterInfo.Name = UMaterialExpressionTextureSetSampleParameter::MakeConstantParameterName(CustomParameter.ParameterInfo.Name, ParameterName);
			Parameter.ParameterInfo.Association = CustomParameter.ParameterInfo.Association;
			Parameter.ParameterInfo.Index = CustomParameter.ParameterInfo.Index;
			VectorParameters.Add(Parameter);
		}
	}
}

void UTextureSet::PreSave(FObjectPreSaveContext SaveContext)
{
#if WITH_EDITOR
	// Important to call UpdateDerivedData before Super, otherwise it causes 
	// the package to fail to save if any of the derived data's references
	// have changed.
	UpdateDerivedData();
#endif

	Super::PreSave(SaveContext);
}

void UTextureSet::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	FixupData();
	UpdateDerivedData();
#endif
}

void UTextureSet::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	UTextureSetDefinition::FOnTextureSetDefinitionChangedEvent.Remove(OnTextureSetDefinitionChangedHandle);
#endif
}

#if WITH_EDITOR
EDataValidationResult UTextureSet::IsDataValid(FDataValidationContext& Context)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!IsValid(Definition))
	{
		Context.AddError(LOCTEXT("MissingDefinition","A texture set must reference a valid definition."));
		Result = EDataValidationResult::Invalid;
	}

	return CombineDataValidationResults(Result, Super::IsDataValid(Context));
}
#endif

#if WITH_EDITOR
void UTextureSet::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (IsValid(Definition))
	{
		OutTags.Add(FAssetRegistryTag("TextureSetDefinition", Definition->GetName(), FAssetRegistryTag::TT_Alphabetical));
		OutTags.Add(FAssetRegistryTag("TextureSetDefinitionID", Definition->GetGuid().ToString(), FAssetRegistryTag::TT_Hidden));
	}

	Super::GetAssetRegistryTags(OutTags);
}
#endif

#if WITH_EDITOR
void UTextureSet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName ChangedPropName = PropertyChangedEvent.GetPropertyName();

	if (ChangedPropName == GET_MEMBER_NAME_CHECKED(UTextureSet, Definition)
		&& PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		FixupData();
	}
}
#endif

#if WITH_EDITOR
void UTextureSet::FixupData()
{
	// Only fixup the data if we have a valid definition. Otherwise leave it as-is so it's there for when we do.
	if (IsValid(Definition))
	{
		// Source Textures
		TMap<FName, TObjectPtr<UTexture>> NewSourceTextures;

		for (const auto& [Name, TextureInfo] : Definition->GetModuleInfo().GetSourceTextures())
		{
			TObjectPtr<UTexture>* OldTexture = SourceTextures.Find(Name);
			NewSourceTextures.Add(Name, (OldTexture != nullptr) ? *OldTexture : nullptr);
		}

		SourceTextures = NewSourceTextures;

		// Update Asset Params
		AssetParams.UpdateParamList(this, Definition->GetRequiredAssetParamClasses());
	}
}
#endif

#if WITH_EDITOR
void UTextureSet::UpdateDerivedData()
{
	bIsDerivedDataReady = false;

	FDataValidationContext ValidationContext;
	if (!IsValid(Definition) || Definition->IsDataValid(ValidationContext) == EDataValidationResult::Invalid)
	{
		// If we have no definition, clear our derived data
		DerivedData = nullptr;
		DerivedTextures.Empty();
		return;
	}

	if (!ensureAlways(!ActiveCooker.IsValid()))
	{
		// This generally shouldn't happen, but if it does, make sure we finish the previous cook before trying to kick off another one
		if (!TryCancelCook())
		{
			FTextureSetCompilingManager::Get().FinishCompilation({this});
		}
	}

	check(!ActiveCooker.IsValid());

	ActiveCooker = MakeShared<TextureSetCooker>(this);

	if (!ActiveCooker->CookRequired())
	{
		bIsDerivedDataReady = true;
		ActiveCooker = nullptr;
	}
	else
	{
		if (FTextureSetCompilingManager::Get().IsAsyncCompilationAllowed(this))
		{
			// Will swap out material instances with default values until cooking has finished
			NotifyMaterialInstances();

			FTextureSetCompilingManager::Get().StartCompilation({this});
		}
		else
		{
			ActiveCooker->Execute();
			OnFinishCook();
		}
	}
}
#endif

#if WITH_EDITOR
bool UTextureSet::IsAsyncCookComplete() const
{
	if (ActiveCooker.IsValid())
	{
		if (ActiveCooker->IsAsyncJobInProgress())
			return false;
		else
			return true;
	}

	return false;
}
#endif

#if WITH_EDITOR
bool UTextureSet::TryCancelCook()
{
	if (ActiveCooker.IsValid() && ActiveCooker->TryCancel())
	{
		ActiveCooker = nullptr;
		return true;
	}

	return false;
}
#endif

#if WITH_EDITOR
void UTextureSet::NotifyMaterialInstances()
{
	for (TObjectIterator<UMaterialInstance> It; It; ++It)
	{
		for (FCustomParameterValue& Param : It->CustomParameterValues)
		{
			if (Param.ParameterValue == this)
			{
				FPropertyChangedEvent Event(nullptr);
				It->PostEditChangeProperty(Event);
				break;
			}
		}
	}
}
#endif

#if WITH_EDITOR
void UTextureSet::OnFinishCook()
{
	check(IsValid(Definition));
	check(ActiveCooker.IsValid());

	ActiveCooker->Finalize();

	ActiveCooker = nullptr;
	bIsDerivedDataReady = true;

	// Update loaded material instances that use this texture set
	NotifyMaterialInstances();
}
#endif

#if WITH_EDITOR
void UTextureSet::OnDefinitionChanged(UTextureSetDefinition* ChangedDefinition)
{
	if (ChangedDefinition == Definition && !HasAnyFlags(RF_NeedPostLoad))
	{
		FixupData();
		UpdateDerivedData();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
