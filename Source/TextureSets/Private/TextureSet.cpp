// (c) Electronic Arts. All Rights Reserved.

#include "TextureSet.h"

#include "TextureSetDefinition.h"
#include "TextureSetCooker.h"
#include "TextureSetModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/ObjectSaveContext.h"
#include "Engine/TextureDefines.h"
#include "ImageUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "ProfilingDebugging/CookStats.h"
#include "DerivedDataCacheInterface.h"
#include "Serialization/MemoryReader.h"
#if WITH_EDITOR
#include "DerivedDataBuildVersion.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSet"

UTextureSet::UTextureSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{}

void UTextureSet::PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext)
{
	if (ObjectSaveContext.IsCooking())
		return;

	UpdateDerivedData();
}

void UTextureSet::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
#if WITH_EDITOR	
	if (!IsValid(Definition))
		return;

	if (ObjectSaveContext.IsCooking())
		return;
#endif
}

void UTextureSet::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (IsValid(Definition))
	{
		const FString& DefinitionName = Definition->GetName();
		OutTags.Add(FAssetRegistryTag("TextureSetDefinition", DefinitionName, FAssetRegistryTag::TT_Alphabetical));
	}

	Super::GetAssetRegistryTags(OutTags);
}

void UTextureSet::PostLoad()
{
	Super::PostLoad();
	FixupData();
	UpdateDerivedData();
}

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
		return;
	}

	FString NewKey = ComputeTextureSetDataKey();
	if (IsValid(DerivedData) && NewKey == DerivedData->Key)
		return; // Hash key is valid, no rebuild required.

	// Not found in DDC, create a new derived data set for the key
	if (!IsValid(DerivedData))
	{
		FName DerivedDataName = MakeUniqueObjectName(this, UTextureSetDerivedData::StaticClass(), "DerivedData");
		DerivedData = NewObject<UTextureSetDerivedData>(this, DerivedDataName, RF_NoFlags);
	}

	// Garbage collection should destroy the unused cooked textures when all references from material instance are removed
	// TODO: Verify this happens with the RF_Standalone flag set (I'm not sure it will)
	DerivedTextures.SetNum(Definition->GetNumPackedTexture());

	for (int t = 0; t < DerivedTextures.Num(); t++)
	{
		UTexture* Texture = DerivedTextures[t];

		FName TextureName = FName(GetName() + "_CookedTexture_" + FString::FromInt(t));

		if (Texture == nullptr)
		{
			// Try to find an existing texture stored in our package that may have become unreferenced, but still exists.
			Texture = static_cast<UTexture*>(FindObjectWithOuter(this, nullptr, TextureName));
		}

		if (!IsValid(Texture) || !Texture->IsInOuter(this) || Texture->GetFName() != TextureName)
		{
			Texture = NewObject<UTexture2D>(this, TextureName, RF_Standalone);
		}

		DerivedTextures[t] = Texture;
	}

	// Retreive derived data from the DDC, or cook new data
	TArray<uint8> OutData;
	TextureSetCooker* Cooker = new TextureSetCooker(this);
	bool bDataWasBuilt = false;
	//COOK_STAT(auto Timer = NavCollisionCookStats::UsageStats.TimeSyncWork());
	if (GetDerivedDataCacheRef().GetSynchronous(Cooker, OutData, &bDataWasBuilt))
	{
		//COOK_STAT(Timer.AddHitOrMiss(bDataWasBuilt ? FCookStats::CallStats::EHitOrMiss::Miss : FCookStats::CallStats::EHitOrMiss::Hit, OutData.Num()));
		// If data was built, the cooker has already configured our derived data for us.
		// If data was retreived from the cache, we have to de-serialized it.
		if (!bDataWasBuilt)
		{
			check(OutData.Num());

			// Deserialize the derived data
			FMemoryReader MemoryReaderAr(OutData);
			DerivedData->Serialize(MemoryReaderAr);

			// Recover the textures
			for (int t = 0; t < DerivedData->PackedTextureData.Num(); t++)
			{
				const FPackedTextureData& TextureData = DerivedData->PackedTextureData[t];
				// Setting the source ID to the same value as is set by the TextureSetCooker means
				// the DDC should be able to locate the existing cooked data, even if our source
				// contains no data.
				DerivedTextures[t]->Source.SetId(TextureData.Id, true);

				// TODO: Validate texture has cooked data, otherwise we need to regenerate the source of this texture
				// TODO: Texture source data can be cached seperately from the Derived Data, since it's only an intermediate representation
			}
		}
	}

	// TODO: Trigger an update of the material instances
}

#undef LOCTEXT_NAMESPACE
