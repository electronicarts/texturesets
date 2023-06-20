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

static TAutoConsoleVariable<int32> CVarTextureSetParallelCook(
	TEXT("r.TextureSet.ParallelCook"),
	1,
	TEXT("Execute the texture cooking across multiple threads in parallel when possible"),
	ECVF_Default);

#define LOCTEXT_NAMESPACE "TextureSet"

UTextureSet::UTextureSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{}

void UTextureSet::PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext)
{
	if (ObjectSaveContext.IsCooking())
		return;

	UpdateTextureData();
}

void UTextureSet::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
#if WITH_EDITOR	
	if (!IsValid(Definition))
		return;

	if (ObjectSaveContext.IsCooking())
		return;

	UpdateResource();
#endif
}

void UTextureSet::UpdateCookedTextures()
{
#if WITH_EDITOR	
	if (!IsValid(Definition))
		return;

	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();

	// Garbage collection will destroy the unused cooked textures when all references from material instance are removed
	PackedTextureData.SetNum(PackingInfo.NumPackedTextures());

	for (int t = 0; t < PackingInfo.NumPackedTextures(); t++)
	{
		UTexture* PackedTexture = GetPackedTexture(t);

		FName TextureName = FName(GetName() + "_CookedTexture_" + FString::FromInt(t));

		if (PackedTexture == nullptr)
		{
			PackedTexture = static_cast<UTexture*>(FindObjectWithOuter(this, nullptr, TextureName));
			PackedTextureData[t].Texture = PackedTexture;
		}

		if (!IsValid(PackedTexture) || !PackedTexture->IsInOuter(this) || PackedTexture->GetFName() != TextureName)
		{
			// Reset to default
			FObjectDuplicationParameters DuplicateParams = InitStaticDuplicateObjectParams(Definition->GetDefaultPackedTexture(t), this, TextureName);
			DuplicateParams.bSkipPostLoad = true;
			PackedTexture = Cast<UTexture>(StaticDuplicateObjectEx(DuplicateParams));
			PackedTexture->SetFlags(RF_NoFlags);
			PackedTextureData[t].Texture = PackedTexture;
		}
	}

	for (int32 PackedTextureIndex = 0; PackedTextureIndex < PackingInfo.NumPackedTextures(); PackedTextureIndex++)
	{
		UTexture* CookedTexture = GetPackedTexture(PackedTextureIndex);

		const FTextureSetPackedTextureDef& Def = PackingInfo.GetPackedTextureDef(PackedTextureIndex);

		bool AnyChanges = false;
		if (CookedTexture->CompressionSettings != Def.CompressionSettings)
		{
			CookedTexture->CompressionSettings = Def.CompressionSettings;
			AnyChanges = true;
		}

		if (AnyChanges)
		{
			CookedTexture->Modify();
			CookedTexture->UpdateResource();
		}
	}

#endif //WITH_EDITOR
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
	UpdateTextureData();
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

	// Tag for debugging, easily force rebuild
	if (!AssetTag.IsEmpty())
		PackedTextureDataKey += AssetTag;

	return PackedTextureDataKey;
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

	return NewTextureSetDataKey;
}

void UTextureSet::UpdateTextureData()
{
	// TODO: Async cooking
	CookImmediate(false);
}

void UTextureSet::UpdateResource()
{
	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();

	for (int32 PackedTextureIndex = 0; PackedTextureIndex < PackingInfo.NumPackedTextures(); PackedTextureIndex++)
	{
		UTexture* CookedTexture = GetPackedTexture(PackedTextureIndex);
		CookedTexture->UpdateResource();
	}
}

const TMap<FName, FVector4> UTextureSet::GetMaterialParameters()
{
	TMap<FName, FVector4> CombinedParams;

	for (FPackedTextureData PackedData : PackedTextureData)
	{
		CombinedParams.Append(PackedData.MaterialParameters);
	}

	return CombinedParams;
}

void UTextureSet::CookImmediate(bool Force)
{
	check(IsValid(Definition));

	UpdateCookedTextures();
	FString NewTextureSetDataKey = ComputeTextureSetDataKey();
	if ((NewTextureSetDataKey != TextureSetDataKey) || Force)
	{
		TextureSetDataKey = NewTextureSetDataKey;

		TextureSetCooker LocalCooker(this);

		ParallelFor(PackedTextureData.Num(), [this, LocalCooker](int32 i)
		{
			PackedTextureData[i].Key = ComputePackedTextureKey(i);
			PackedTextureData[i].MaterialParameters.Empty();
			LocalCooker.PackTexture(i, PackedTextureData[i].MaterialParameters);

		}, CVarTextureSetParallelCook.GetValueOnAnyThread() ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);

		for (int i = 0; i < PackedTextureData.Num(); i++)
			PackedTextureData[i].Texture->Modify(true);

		UpdateResource();
	}
}

#undef LOCTEXT_NAMESPACE
