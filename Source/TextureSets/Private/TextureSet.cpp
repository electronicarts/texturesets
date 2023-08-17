// (c) Electronic Arts. All Rights Reserved.

#include "TextureSet.h"

#include "TextureSetDefinition.h"
#include "TextureSetCooker.h"
#include "TextureSetModule.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/ObjectSaveContext.h"
#include "Engine/TextureDefines.h"
#include "ImageUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "ProfilingDebugging/CookStats.h"
#include "DerivedDataCacheInterface.h"
#include "Serialization/MemoryReader.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "DerivedDataBuildVersion.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

UTextureSet::UTextureSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{}

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

void UTextureSet::PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext)
{
	Super::PreSaveRoot(ObjectSaveContext);

	UpdateDerivedData();
}

void UTextureSet::PostLoad()
{
	Super::PostLoad();
	FixupData();
	UpdateDerivedData();
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
		FixupData();
	}
}
#endif

void UTextureSet::FixupData()
{
#if WITH_EDITOR
	// Only fixup the data if we have a valid definition. Otherwise leave it as-is so it's there for when we do.
	if (IsValid(Definition))
	{
		// Source Textures
		TMap<FName, TObjectPtr<UTexture>> NewSourceTextures;

		for (auto& TextureInfo : Definition->GetSharedInfo().GetSourceTextures())
		{
			TObjectPtr<UTexture>* OldTexture = SourceTextures.Find(TextureInfo.Name);
			NewSourceTextures.Add(TextureInfo.Name, (OldTexture != nullptr) ? *OldTexture : nullptr);
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
#endif
}

FString UTextureSet::ComputePackedTextureKey(int PackedTextureIndex) const
{
	check(IsValid(Definition));

	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();
	check(PackedTextureIndex < PackingInfo.NumPackedTextures());

	// All data hash keys start with a global version tracking format changes
	FString PackedTextureDataKey("PACKING_VER1_");

	// Hash all the source data.
	// We currently don't have a mechanism to know which specific source textures we depend on
	// TODO: Only hash on source textures that contribute to this packed texture
	for (const auto& [Name, SourceTexture] : SourceTextures)
	{
		if (IsValid(SourceTexture))
			PackedTextureDataKey += Name.ToString() + "<" + SourceTexture->Source.GetId().ToString() + ">_";
	}
	
	// Hash the packing def
	const FTextureSetPackedTextureDef& PackedTextureDef = PackingInfo.GetPackedTextureDef(PackedTextureIndex);
	PackedTextureDataKey += PackedTextureDef.ComputeHashKey() + "_";

	// Hash the modules
	for (const UTextureSetModule* Module : Definition->GetModules())
	{
		PackedTextureDataKey += Module->GetName() + "<" + FString::FromInt(Module->ComputeProcessingHash()) + ">_";
	}

	// Key for debugging, easily force rebuild
	if (!UserKey.IsEmpty())
		PackedTextureDataKey += "UserKey<" + UserKey + ">";

	UE::DerivedData::FBuildVersionBuilder IdBuilder;
	IdBuilder << PackedTextureDataKey;

	const FGuid NewID = IdBuilder.Build();

	return NewID.ToString();
}

FString UTextureSet::ComputeTextureSetDataKey() const
{
	if (!IsValid(Definition))
		return "TEXTURE_SET_INVALID_DEFINITION";

	// All data hash keys start with a global version tracking format changes
	FString NewTextureSetDataKey("TEXTURE_SET_V1_");

	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();
	for (int32 i = 0; i < PackingInfo.NumPackedTextures(); i++)
	{
		NewTextureSetDataKey += ComputePackedTextureKey(i);
	}

	UE::DerivedData::FBuildVersionBuilder IdBuilder;
	IdBuilder << NewTextureSetDataKey;

	const FGuid NewID = IdBuilder.Build();

	return NewID.ToString();
}

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
	DerivedTextures.SetNum(Definition->GetNumPackedTexture());

	for (int t = 0; t < DerivedTextures.Num(); t++)
	{
		FName TextureName = FName(GetName() + "_CookedTexture_" + FString::FromInt(t));

		if (!IsValid(DerivedTextures[t]))
		{
			// Try to find an existing texture stored in our package that may have become unreferenced, but still exists.
			DerivedTextures[t] = static_cast<UTexture*>(FindObjectWithOuter(this, nullptr, TextureName));
		}

		if (!IsValid(DerivedTextures[t]) || !DerivedTextures[t]->IsInOuter(this) || DerivedTextures[t]->GetFName() != TextureName)
		{
			// Create a new texture if none exists
			DerivedTextures[t] = NewObject<UTexture2D>(this, TextureName, RF_NoFlags);
		}
		DerivedTextures[t]->SetFlags(RF_Public); // TODO: Remove this flag when we can clear references to these textures from the material instances in serialized data
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
	FString NewKey = ComputeTextureSetDataKey();
	if (NewKey != DerivedData->Key)
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

#undef LOCTEXT_NAMESPACE
