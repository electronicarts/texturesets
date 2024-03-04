// (c) Electronic Arts. All Rights Reserved.

#include "TextureSet.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "TextureSetDefinition.h"
#include "TextureSetDerivedData.h"
#include "TextureSetsHelpers.h"
#include "UObject/ObjectSaveContext.h"
#include "Serialization/Archive.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "TextureSetCompilingManager.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

UTextureSet::UTextureSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
, DerivedData(nullptr)
{
#if WITH_EDITOR
	OnTextureSetDefinitionChangedHandle = UTextureSetDefinition::FOnTextureSetDefinitionChangedEvent.AddUObject(this, &UTextureSet::OnDefinitionChanged);
#endif
}

#if WITH_EDITOR
void UTextureSet::AddSource(FName SourceName, UTexture* Texture)
{
	FTextureSetSourceTextureReference SourceReferene;
	SourceReferene.Texture = Texture;
	SourceTextures.Add(SourceName, SourceReferene);	
}
#endif

#if WITH_EDITOR
void UTextureSet::RemoveSource(FName SourceName)
{
	SourceTextures.Remove(SourceName);
}
#endif

#if WITH_EDITOR
TArray<FName> UTextureSet::GetSourceNames()
{
	TArray<FName> Result;
	SourceTextures.GetKeys(Result);
	return Result;
}
#endif

#if WITH_EDITOR
bool UTextureSet::IsCompiling() const
{
	return FTextureSetCompilingManager::Get().IsRegistered(this);
}
#endif

void UTextureSet::AugmentMaterialTextureParameters(const FCustomParameterValue& CustomParameter, TArray<FTextureParameterValue>& TextureParameters) const
{
	if (!IsValid(Definition))
		return;

#if WITH_EDITOR
	if (!DerivedData)
	{
		if (FApp::CanEverRender())
		{
			// Fall back to the default texture set if possible
			if(IsValid(Definition->GetDefaultTextureSet()) && !this->IsDefaultTextureSet())
			{
				Definition->GetDefaultTextureSet()->AugmentMaterialTextureParameters(CustomParameter, TextureParameters);
			}
			return;
		}
		else
		{
			// Likely we're in a commandlet and cooking, so wait until we have valid derived data
			((UTextureSet*)this)->UpdateDerivedData(false);
		}
	}
#endif

	check(DerivedData);

	for (int i = 0; i < DerivedData->Textures.Num(); i++)
	{
		const FDerivedTextureData& DerivedTextureData = DerivedData->Textures[i].Data;

		// Set the texture parameter for each packed texture
		FTextureParameterValue TextureParameter;
		TextureParameter.ParameterValue = DerivedData->Textures[i].Texture;
		TextureParameter.ParameterInfo.Name = TextureSetsHelpers::MakeTextureParameterName(CustomParameter.ParameterInfo.Name, i);
		TextureParameter.ParameterInfo.Association = CustomParameter.ParameterInfo.Association;
		TextureParameter.ParameterInfo.Index = CustomParameter.ParameterInfo.Index;
		TextureParameters.Add(TextureParameter);
	}
}

void UTextureSet::AugmentMaterialVectorParameters(const FCustomParameterValue& CustomParameter, TArray<FVectorParameterValue>& VectorParameters) const
{
	if (!IsValid(Definition))
		return;

#if WITH_EDITOR
	if (!DerivedData)
	{
		if (FApp::CanEverRender())
		{
			// Fall back to the default texture set if possible
			if (IsValid(Definition->GetDefaultTextureSet()) && !this->IsDefaultTextureSet())
			{
				Definition->GetDefaultTextureSet()->AugmentMaterialVectorParameters(CustomParameter, VectorParameters);
			}
			return;
		}
		else
		{
			// Likely we're in a commandlet and cooking, so wait until we have valid derived data
			((UTextureSet*)this)->UpdateDerivedData(false);
		}
	}
#endif

	check(DerivedData);

	// Set any constant parameters what we have
	for (const auto& [ParameterName, Data] : DerivedData->MaterialParameters)
	{
		FVectorParameterValue Parameter;
		Parameter.ParameterValue = FLinearColor(Data.Value);
		Parameter.ParameterInfo.Name = TextureSetsHelpers::MakeConstantParameterName(CustomParameter.ParameterInfo.Name, ParameterName);
		Parameter.ParameterInfo.Association = CustomParameter.ParameterInfo.Association;
		Parameter.ParameterInfo.Index = CustomParameter.ParameterInfo.Index;
		VectorParameters.Add(Parameter);
	}

	for (int i = 0; i < DerivedData->Textures.Num(); i++)
	{
		const FDerivedTextureData& DerivedTextureData = DerivedData->Textures[i].Data;

		// Set any constant parameters that come with this texture
		for (auto& [ParameterName, Value] : DerivedTextureData.TextureParameters)
		{
			FVectorParameterValue Parameter;
			Parameter.ParameterValue = FLinearColor(Value);
			Parameter.ParameterInfo.Name = TextureSetsHelpers::MakeConstantParameterName(CustomParameter.ParameterInfo.Name, ParameterName);
			Parameter.ParameterInfo.Association = CustomParameter.ParameterInfo.Association;
			Parameter.ParameterInfo.Index = CustomParameter.ParameterInfo.Index;
			VectorParameters.Add(Parameter);
		}
	}
}

void UTextureSet::Serialize(FArchive& Ar)
{
	// Only serialize derived data if cooking/cooked, or if we're a default texture set.
	if (Ar.IsSaving())
		bSerializeDerivedData = Ar.IsCooking() || IsDefaultTextureSet();

	Super::Serialize(Ar);

	if (bSerializeDerivedData)
	{
		if (Ar.IsCooking() && Ar.IsSaving())
			check(DerivedData);
		
		Ar << DerivedData;
	}
}

void UTextureSet::PreSave(FObjectPreSaveContext SaveContext)
{
#if WITH_EDITOR
	// Important to call UpdateDerivedData before Super, otherwise it causes 
	// the package to fail to save if any of the derived data's references
	// have changed.
	// Don't allow async update if we're cooking
	UpdateDerivedData(!SaveContext.IsCooking());
#endif

	Super::PreSave(SaveContext);
}

void UTextureSet::PostLoad()
{
	Super::PostLoad();

	if (DerivedData)
		DerivedData->ConditionalPostLoad();

#if WITH_EDITOR
	UpdateDerivedData(true);
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
EDataValidationResult UTextureSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!IsValid(Definition))
	{
		Context.AddError(LOCTEXT("MissingDefinition","A texture set must reference a valid definition."));
		Result = EDataValidationResult::Invalid;
	}
	else
	{
		FDataValidationContext DefinitionContext;
		if (Definition->IsDataValid(DefinitionContext) == EDataValidationResult::Invalid)
		{
			Context.AddError(FText::Format(LOCTEXT("InvalidDefinition", "The referenced definition ({0}) has invalid data."), FText::FromString(Definition->GetName())));
			Result = EDataValidationResult::Invalid;
		}
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

		// Update derived data whenever a definition changes.
		// Avoids an error when the definition changes on save and has a mismatch between harvested references and serialized derived data.
		UpdateDerivedData(true, true);
	}
}
#endif

#if WITH_EDITOR
void UTextureSet::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	// Important to start immediately (2nd arg) so the derived textures get initialized with a valid format
	// TODO: Currently can't execute async, because we need to call BeginCacheForCookedPlatformData when the derived textures have valid data
	UpdateDerivedData(true, true);
}
#endif

#if WITH_EDITOR
bool UTextureSet::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{
	if (!IsValid(Definition))
		return true; // As loaded as we'll get without a definition

	if (IsCompiling() || !DerivedData)
		return false;

	return true;
}
#endif

#if WITH_EDITOR
void UTextureSet::ClearCachedCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	// We don't have any platform specific platform data
}
#endif

#if WITH_EDITOR
void UTextureSet::ClearAllCachedCookedPlatformData()
{
	// Don't actually clear it, because we have other systems that rely on us being properly initialized
	DerivedData = nullptr;
}
#endif

#if WITH_EDITOR
void UTextureSet::FixupData()
{
	// Only fixup the data if we have a valid definition. Otherwise leave it as-is so it's there for when we do.
	if (IsValid(Definition))
	{
		// Source Textures
		TMap<FName, FTextureSetSourceTextureReference> NewSourceTextures;

		for (const auto& [Name, TextureInfo] : Definition->GetModuleInfo().GetSourceTextures())
		{
			if (SourceTextures.Contains(Name))
			{
				NewSourceTextures.Add(Name, SourceTextures.FindChecked(Name));
			}
			else if (UnusedSourceTextures.Contains(Name))
			{
				NewSourceTextures.Add(Name, UnusedSourceTextures.FindAndRemoveChecked(Name));
			}
			else
			{
				NewSourceTextures.Add(Name, FTextureSetSourceTextureReference());
			}
		}

		// If any source textures are unused and pointing to valid resources, save them as unused sources
		for (const auto& [Name, TextureInfo] : SourceTextures)
		{
			if (!NewSourceTextures.Contains(Name) && !TextureInfo.Texture)
			{
				UnusedSourceTextures.Add(Name, TextureInfo);
			}
		}

		SourceTextures = NewSourceTextures;

		// Update Asset Params
		AssetParams.UpdateParamList(this, Definition->GetRequiredAssetParamClasses());
	}

	// Ensure any textures that are hanging around in our package due to the way we used to handle
	// derived data at least have valid sources to avoid warnings (they will eventually be cleaned
	// up as they are unreferenced)
	TArray<UObject*> Objects;
	const bool bIncludeNestedObjects = false;
	GetObjectsWithPackage(this->GetOutermost(), Objects);

	for (UObject* Object : Objects)
	{
		UTexture* Texture = Cast<UTexture>(Object);

		if (IsValid(Texture) && (!DerivedData || Object->GetOuter() != DerivedData))
		{
			if (!Texture->Source.IsValid())
			{
				UE_LOG(LogTextureSet, Log, TEXT("%s: Found texture %s in package with invalid source data. Intitializing with 4x4 black texture to avoid warnings"), *this->GetName(), *Texture->GetName());
				uint8 BlackPixels[4*4] = { 0 };
				Texture->Source.Init(4, 4, 1, 1, TSF_G8, BlackPixels);
			}
		}
	}
}
#endif

#if WITH_EDITOR
void UTextureSet::UpdateDerivedData(bool bAllowAsync, bool bStartImmediately)
{
	FixupData();

	FDataValidationContext ValidationContext;
	if (!IsValid(Definition) || Definition->IsDataValid(ValidationContext) == EDataValidationResult::Invalid)
	{
		// If we have no definition, clear our derived data
		DerivedData = nullptr;
		return;
	}

	FTextureSetCompilingManager& CompilingManager = FTextureSetCompilingManager::Get();

	bool bAsync = bAllowAsync && !IsDefaultTextureSet();

	if (!bAsync || bStartImmediately)
	{
		// StartCompilation not QueueCompilation, so compilation is kicked off immediately
		CompilingManager.StartCompilation(this, bAsync);
	}
	else
	{
		// Add it to the Queue, will be picked up when possible
		CompilingManager.QueueCompilation(this);
	}
}
#endif

const UTextureSetDerivedData* UTextureSet::GetDerivedData() const
{
	#if WITH_EDITOR
	// If called in the editor it's possible we haven't updated our derived data yet, so stall here until we do.
	((UTextureSet*)this)->UpdateDerivedData(false, true);
	#endif

	check(DerivedData);
	return DerivedData.Get();
}

bool UTextureSet::IsDefaultTextureSet() const
{
	return IsValid(Definition) && Definition->GetDefaultTextureSet() == this;
}


#if WITH_EDITOR
void UTextureSet::OnDefinitionChanged(UTextureSetDefinition* ChangedDefinition)
{
	if (ChangedDefinition == Definition && !HasAnyFlags(RF_NeedPostLoad))
	{
		// Default texture set gets it's derived data updated immediately, so doesn't need to do anything here.
		if (IsDefaultTextureSet())
			return;

		UpdateDerivedData(true);
	}
}
#endif

#undef LOCTEXT_NAMESPACE
