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
#include "DerivedDataCacheInterface.h"
#include "Misc/DataValidation.h"
#include "DerivedDataBuildVersion.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

UTextureSet::UTextureSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
#if WITH_EDITOR
	OnTextureSetDefinitionChangedHandle = UTextureSetDefinition::FOnTextureSetDefinitionChangedEvent.AddUObject(this, &UTextureSet::OnDefinitionChanged);
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

void UTextureSet::AugmentMaterialParameters(const FCustomParameterValue& CustomParameter, TArray<FVectorParameterValue>& VectorParameters, TArray<FTextureParameterValue>& TextureParameters) const
{
	// Set any constant parameters what we have
	for (auto& [ParameterName, Value] : DerivedData->MaterialParameters)
	{
		FVectorParameterValue Parameter;
		Parameter.ParameterValue = FLinearColor(Value);
		Parameter.ParameterInfo.Name = UMaterialExpressionTextureSetSampleParameter::MakeConstantParameterName(CustomParameter.ParameterInfo.Name, ParameterName);
		Parameter.ParameterInfo.Association = CustomParameter.ParameterInfo.Association;
		Parameter.ParameterInfo.Index = CustomParameter.ParameterInfo.Index;
		VectorParameters.Add(Parameter);
	}

	for (int i = 0; i < DerivedData->PackedTextureData.Num(); i++)
	{
		const FPackedTextureData& PackedTextureData = DerivedData->PackedTextureData[i];

		// Set the texture parameter for each packed texture
		FTextureParameterValue TextureParameter;
		TextureParameter.ParameterValue = DerivedTextures[i].Get();
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
void UTextureSet::OnDefinitionChanged(UTextureSetDefinition* ChangedDefinition)
{
	if (ChangedDefinition == Definition)
	{
		FixupData();
		UpdateDerivedData();
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
				AssetParams.Add(NewObject<UTextureSetAssetParams>(this, SampleParamClass));
			}
		}
	}
}
#endif

#if WITH_EDITOR
void UTextureSet::UpdateDerivedData()
{
	if (!IsValid(Definition))
	{
		// If we have no definition, clear our derived data
		DerivedData = nullptr;
		DerivedTextures.Empty();
		return;
	}

	// Garbage collection will destroy the unused cooked textures when all references from material instance are removed
	DerivedTextures.SetNum(Definition->GetPackingInfo().NumPackedTextures());

	for (int t = 0; t < DerivedTextures.Num(); t++)
	{
		FName TextureName = FName(GetName() + "_CookedTexture_" + FString::FromInt(t));

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

		check(DerivedTextures[t]->IsInOuter(this));
		check(DerivedTextures[t]->GetFName() == TextureName);
	}

	// Create a new derived data if ours is missing
	if (!IsValid(DerivedData))
	{
		FName DerivedDataName = MakeUniqueObjectName(this, UTextureSetDerivedData::StaticClass(), "DerivedData");
		DerivedData = NewObject<UTextureSetDerivedData>(this, DerivedDataName, RF_NoFlags);
	}

	TSharedRef<TextureSetCooker> Cooker = MakeShared<TextureSetCooker>(this);

	bool bDataWasBuilt = false;

	// Fetch or build derived data for the texture set
	FGuid NewID = ComputeTextureSetDataId();
	if (NewID != DerivedData->Id)
	{
		Modify();

		// Keys to current derived data don't match
		// Retreive derived data from the DDC, or cook new data
		TArray<uint8> Data;
		FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();
		//COOK_STAT(auto Timer = NavCollisionCookStats::UsageStats.TimeSyncWork());
		if (DDC.GetSynchronous(new TextureSetDerivedDataPlugin(Cooker), Data, &bDataWasBuilt))
		{
			//COOK_STAT(Timer.AddHitOrMiss(bDataWasBuilt ? FCookStats::CallStats::EHitOrMiss::Miss : FCookStats::CallStats::EHitOrMiss::Hit, OutData.Num()));
			if (bDataWasBuilt)
			{
				// If data was built, the cooker has already configured our derived data for us.
				// Update the texture resource so we see it reflected in the editor right away
				for (int t = 0; t < DerivedTextures.Num(); t++)
				{
					DerivedTextures[t]->UpdateResource();
				}
			}
			else
			{
				// If data was retreived from the cache, de-serialized it.
				FMemoryReader MemoryReaderDerivedData(Data);
				DerivedData->Serialize(MemoryReaderDerivedData);
			}
		}
	}

	// Ensure our textures are up to date
	if (!bDataWasBuilt)
	{
		for (int t = 0; t < DerivedTextures.Num(); t++)
		{
			Cooker->UpdateTexture(t);

			// This can happen if a texture build gets interrupted after a texture-set cook.
			if (!DerivedTextures[t]->PlatformDataExistsInCache())
			{
				// Build this texture to regenerate the source data, so it can be cached.
				Cooker->BuildTextureData(t);
			}
			DerivedTextures[t]->UpdateResource();
		}
	}

	// TODO: Trigger an update of the material instances
}
#endif

#undef LOCTEXT_NAMESPACE
