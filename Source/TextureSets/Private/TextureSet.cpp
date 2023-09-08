// (c) Electronic Arts. All Rights Reserved.

#include "TextureSet.h"

#include "TextureSetDefinition.h"
#include "TextureSetCooker.h"
#include "TextureSetModule.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/ObjectSaveContext.h"
#include "Engine/TextureDefines.h"
#include "Serialization/MemoryReader.h"
#if WITH_EDITOR
#include "ImageUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "ProfilingDebugging/CookStats.h"
#include "Misc/DataValidation.h"
#include "DerivedDataBuildVersion.h"
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
	return ActiveCooker.IsValid() && !bIsDerivedDataReady;
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
		checkf(IsValid(Definition->GetDefaultTextureSet()), TEXT("Definition has no default texture set"));
		checkf(Definition->GetDefaultTextureSet() != this, TEXT("Attempting to use default texture set before derived data is ready"));
		checkf(Definition->GetDefaultTextureSet()->bIsDerivedDataReady, TEXT("Attempting to use default texture set before derived data is ready"));

		// Fall back to using the definition's default texture set if our derived data isn't ready yet
		TS = bIsDerivedDataReady ? this : Definition->GetDefaultTextureSet();
	}
#endif

	// Set any constant parameters what we have
	for (auto& [ParameterName, Value] : TS->DerivedData->MaterialParameters)
	{
		FVectorParameterValue Parameter;
		Parameter.ParameterValue = FLinearColor(Value);
		Parameter.ParameterInfo.Name = UMaterialExpressionTextureSetSampleParameter::MakeConstantParameterName(CustomParameter.ParameterInfo.Name, ParameterName);
		Parameter.ParameterInfo.Association = CustomParameter.ParameterInfo.Association;
		Parameter.ParameterInfo.Index = CustomParameter.ParameterInfo.Index;
		VectorParameters.Add(Parameter);
	}

	for (int i = 0; i < TS->DerivedData->PackedTextureData.Num(); i++)
	{
		const FPackedTextureData& PackedTextureData = TS->DerivedData->PackedTextureData[i];

		// Set the texture parameter for each packed texture
		FTextureParameterValue TextureParameter;
		TextureParameter.ParameterValue = TS->DerivedTextures[i].Get();
		TextureParameter.ParameterInfo.Name = UMaterialExpressionTextureSetSampleParameter::MakeTextureParameterName(CustomParameter.ParameterInfo.Name, i);
		TextureParameter.ParameterInfo.Association = CustomParameter.ParameterInfo.Association;
		TextureParameter.ParameterInfo.Index = CustomParameter.ParameterInfo.Index;
		TextureParameters.Add(TextureParameter);

		// Set any constant parameters that come with this texture
		for (auto& [ParameterName, Value] : PackedTextureData.MaterialParameters)
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
	Super::PreSave(SaveContext);

#if WITH_EDITOR
	UpdateDerivedData();
#endif
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
		OnDefinitionChanged(Definition);
	}
}
#endif

#if WITH_EDITOR
FGuid UTextureSet::ComputePackedTextureDataID(int PackedTextureIndex) const
{
	check(IsValid(Definition));
	check(PackedTextureIndex < Definition->GetPackingInfo().NumPackedTextures());

	UE::DerivedData::FBuildVersionBuilder IdBuilder;

	IdBuilder << FString("TextureSetPackedTexture_V1");

	// Key for debugging, easily force rebuild
	IdBuilder << UserKey;

	const FTextureSetProcessingGraph Graph = FTextureSetProcessingGraph(Definition->GetModules());
	const FTextureSetProcessingContext Context = FTextureSetProcessingContext(this);

	// We currently don't have a mechanism to know which specific source textures we depend on
	// TODO: Only hash on source textures that contribute to this packed texture
	for (const auto& [Name, Node] : Graph.GetOutputTextures())
	{
		IdBuilder << Node->ComputeGraphHash();
		IdBuilder << Node->ComputeDataHash(Context);
	}

	IdBuilder << GetTypeHash(Definition->GetPackingInfo().GetPackedTextureDef(PackedTextureIndex));

	return IdBuilder.Build();
}
#endif

#if WITH_EDITOR
FGuid UTextureSet::ComputeTextureSetDataId() const
{
	check(IsValid(Definition))

	UE::DerivedData::FBuildVersionBuilder IdBuilder;

	// Start with a global version tracking format changes
	IdBuilder << FString("TextureSet_V1");

	const FTextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();
	for (int32 i = 0; i < PackingInfo.NumPackedTextures(); i++)
	{
		IdBuilder << ComputePackedTextureDataID(i);
	}

	return IdBuilder.Build();
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

		// Asset Params
		TArray<TSubclassOf<UTextureSetAssetParams>> RequiredAssetParamClasses = Definition->GetRequiredAssetParamClasses();
		TArray<TSubclassOf<UTextureSetAssetParams>> ExistingAssetParamClasses;

		// Remove un-needed asset params
		for (int i = 0; i < AssetParams.Num(); i++)
		{
			UTextureSetAssetParams* AssetParam = AssetParams[i];
			if (!RequiredAssetParamClasses.Contains(AssetParam->StaticClass()))
			{
				AssetParams.RemoveAt(i);
				i--;
			}
			else
			{
				ExistingAssetParamClasses.Add(AssetParam->StaticClass());
			}
		}

		// Add missing asset params
		for (TSubclassOf<UTextureSetAssetParams> SampleParamClass : RequiredAssetParamClasses)
		{
			if (!ExistingAssetParamClasses.Contains(SampleParamClass))
			{
				FName AssetParamsName = MakeUniqueObjectName(this, SampleParamClass, SampleParamClass->GetFName());
				AssetParams.Add(NewObject<UTextureSetAssetParams>(this, SampleParamClass, AssetParamsName, RF_NoFlags));
			}
		}
	}
}
#endif

#if WITH_EDITOR
void UTextureSet::UpdateDerivedData()
{
	bIsDerivedDataReady = false;

	if (!IsValid(Definition))
	{
		// If we have no definition, clear our derived data
		DerivedData = nullptr;
		DerivedTextures.Empty();
		return;
	}

	check(IsValid(Definition));
	check(!ActiveCooker.IsValid());

	// Garbage collection will destroy the unused cooked textures when all references from material instance are removed
	DerivedTextures.SetNum(Definition->GetPackingInfo().NumPackedTextures());

	for (int t = 0; t < DerivedTextures.Num(); t++)
	{
		FName TextureName = FName(GetName() + "_DerivedTexture_" + FString::FromInt(t));

		if (!IsValid(DerivedTextures[t]))
		{
			// Try to find an existing texture stored in our package that may have become unreferenced, but still exists.
			// This can hapen if the number of derived textures was higher, became lower, and then was set higher again,
			// before the previous texture was garbage-collected.
			DerivedTextures[t] = static_cast<UTexture*>(FindObjectWithOuter(this, nullptr, TextureName));

			// No existing texture, create a new one
			if (!IsValid(DerivedTextures[t]))
			{
				DerivedTextures[t] = NewObject<UTexture2D>(this, TextureName, RF_Public);
			}
		}
		
		// Usually happens if the outer texture set is renamed
		if (DerivedTextures[t]->GetFName() != TextureName)
		{
			DerivedTextures[t]->Rename(*TextureName.ToString());
		}

		check(DerivedTextures[t]->GetFName() == TextureName);
		check(DerivedTextures[t]->IsInOuter(this));
	}

	// Create a new derived data if ours is missing
	if (!IsValid(DerivedData))
	{
		FName DerivedDataName = MakeUniqueObjectName(this, UTextureSetDerivedData::StaticClass(), "DerivedData");
		DerivedData = NewObject<UTextureSetDerivedData>(this, DerivedDataName, RF_NoFlags);
	}

	bool bCookRequired = false;

	ActiveCooker = MakeShared<TextureSetCooker>(this);

	if (ComputeTextureSetDataId() != DerivedData->Id)
		bCookRequired = true;

	for (int t = 0; !bCookRequired && t < DerivedTextures.Num(); t++)
	{
		// Make sure the texture is using the right key before checking if it's cached
		ActiveCooker->ConfigureTexture(t);

		if (!DerivedTextures[t]->PlatformDataExistsInCache())
			bCookRequired = true;
	}

	if (!bCookRequired)
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

	check(!ActiveCooker->IsAsyncJobInProgress());

	for (int t = 0; t < DerivedTextures.Num(); t++)
	{
		DerivedTextures[t]->UpdateResource();
		DerivedTextures[t]->UpdateCachedLODBias();
	}

	ActiveCooker = nullptr;
	bIsDerivedDataReady = true;

	// Update loaded material instances that use this texture set
	NotifyMaterialInstances();
}
#endif

#if WITH_EDITOR
void UTextureSet::OnDefinitionChanged(UTextureSetDefinition* ChangedDefinition)
{
	if (ChangedDefinition == Definition)
	{
		FixupData();
		UpdateDerivedData();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
