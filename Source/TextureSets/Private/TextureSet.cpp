// (c) Electronic Arts. All Rights Reserved.

#include "TextureSet.h"

#include "TextureSetDefinition.h"
#include "TextureSetCooker.h"
#include "TextureSetModule.h"
#include "TextureSetModifiersAssetUserData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/ObjectSaveContext.h"
#include "Engine/TextureDefines.h"
#include "ImageUtils.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "TextureSet"

static TAutoConsoleVariable<int32> CVarTextureSetFreeImmediateImages(
	TEXT("r.TextureSet.FreeImmediateImages"),
	1,
	TEXT("If enabled, Texture Set will free the memory of immediate images after resource caching"),
	ECVF_Default);

FGuid UTextureSet::PackedTextureSourceGuid(0xA7820CFB, 0x20A74359, 0x8C542C14, 0x9623CF50);

UTextureSet::UTextureSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{}

void UTextureSet::PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext)
{
	if (ObjectSaveContext.IsCooking())
		return;

	UpdateCookedTextures();
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
	CookedTextures.SetNum(PackingInfo.NumPackedTextures());

	for (int t = 0; t < PackingInfo.NumPackedTextures(); t++)
	{
		UTexture* PackedTexture = GetCookedTexture(t);

		FName TextureName = FName(GetName() + "_CookedTexture_" + FString::FromInt(t));

		if (!IsValid(PackedTexture) || !PackedTexture->IsInOuter(this) || PackedTexture->GetFName() != TextureName)
		{
			// Reset to default
			FObjectDuplicationParameters DuplicateParams = InitStaticDuplicateObjectParams(Definition->GetDefaultPackedTexture(t), this, TextureName);
			DuplicateParams.bSkipPostLoad = true;
			// UTexture::PostLoad will create the D3D resource. 
			// This newly created texture will immediately recreate the D3D resource by CookedTexture->UpdateResource() below
			PackedTexture = Cast<UTexture>(StaticDuplicateObjectEx(DuplicateParams));
			PackedTexture->ClearFlags(RF_Transient);
			CookedTextures[t] = PackedTexture;
		}

		// Check for existing user data
		UTextureSetModifiersAssetUserData* TextureModifier = PackedTexture->GetAssetUserData<UTextureSetModifiersAssetUserData>();
		if (!TextureModifier)
		{
			// Create new user data if needed
			FName AssetUserDataName = FName(TextureName.ToString() + "_AssetUserData");
			TextureModifier = NewObject<UTextureSetModifiersAssetUserData>(PackedTexture, AssetUserDataName, RF_NoFlags);
			PackedTexture->AddAssetUserData(TextureModifier);
		}
	}

	for (int32 PackedTextureIndex = 0; PackedTextureIndex < PackingInfo.NumPackedTextures(); PackedTextureIndex++)
	{
		UTexture* CookedTexture = GetCookedTexture(PackedTextureIndex);

		const FTextureSetPackedTextureDef& Def = PackingInfo.GetPackedTextureDef(PackedTextureIndex);

		bool AnyChanges = false;
		if (CookedTexture->CompressionSettings != Def.CompressionSettings)
		{
			CookedTexture->CompressionSettings = Def.CompressionSettings;
			AnyChanges = true;
		}
		UTextureSetModifiersAssetUserData* TextureModifier = CookedTexture->GetAssetUserData<UTextureSetModifiersAssetUserData>();
		// Only after TextureModifier is properly initialized, CookedTexture can be processed correctly.
		// If any data changes on TextureModifier occur, then call Modify and UpdateResource again.
		// For now, this should only happen on the new CookedTexture created above.
		if (TextureModifier->TextureSet != this)
		{
			TextureModifier->TextureSet = this;
			AnyChanges = true;
		}
		if (TextureModifier->PackedTextureDefIndex != PackedTextureIndex)
		{
			TextureModifier->PackedTextureDefIndex = PackedTextureIndex;
			AnyChanges = true;
		}

		if (AnyChanges)
		{
			CookedTexture->Source.SetId(GetPackedTextureSourceGuid(), false);
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
	UpdateCookedTextures();
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

FString UTextureSet::ComputePackedTextureKey(int PackedTextureIndex)
{
	if (!IsValid(Definition))
		return TEXT("INVALID_TEXTURE_SET_DEFINITION");

	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();
	if (PackedTextureIndex >= PackingInfo.NumPackedTextures())
		return TEXT("INVALID_PACKED_TEXTURE_ASSET_INDEX");

	// All data hash keys start with a global version tracking format changes
	FString PackedTextureDataKey("TEXTURE_SET_PACKING_VER1_");

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

TArray<FString> UTextureSet::ComputePackedTextureKeys()
{
	TArray<FString> NewPackedTextureKeys;

	if (!IsValid(Definition))
		return NewPackedTextureKeys;

	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();
	for (int32 i = 0; i < PackingInfo.NumPackedTextures(); i++)
	{
		NewPackedTextureKeys.Add(ComputePackedTextureKey(i));
	}

	return NewPackedTextureKeys;
}

void UTextureSet::ModifyTextureSource(int PackedTextureDefIndex, UTexture* TextureAsset)
{
	if (!IsValid(Definition))
		return;

	if (PackedTextureDefIndex >= CookedTextures.Num())
	{
		checkf(false, TEXT("Invalid Index of Packed Texture within TextureSet."));
		return;
	}

	UTexture* CookedTexture = GetCookedTexture(PackedTextureDefIndex);
	if (CookedTexture != TextureAsset)
	{
		checkf(false, TEXT("Invalid Cooked Texture within TextureSet."));
		return;
	}
	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();
	const FTextureSetPackedTextureDef& TextureSetPackedTextureDef = PackingInfo.GetPackedTextureDef(PackedTextureDefIndex);
	if (TextureSetPackedTextureDef.UsedChannels() == 0)
	{
		checkf(false, TEXT("Invalid Packing within TextureSetDefinition."));
		return;
	}

	// Preprocessing
	{
		FScopeLock Lock(&TextureSetCookCS);

		if (!Cooker.IsValid())
		{
			// First texture that cooks will check if the packed textures need to be updated, and if so create a cooker.
			// Others threads will wait due to above CS for the cooker to be prepared, and then execute the pack in parallel.
			TArray<FString> NewPackedTextureKeys = ComputePackedTextureKeys();
			if (NewPackedTextureKeys != PackedTextureKeys)
			{
				PackedTextureKeys = NewPackedTextureKeys;
				MaterialParameters.Empty();

				Cooker = MakeUnique<TextureSetCooker>(this);
				CookedTexturesProcessedBitmask = 0;
				
				Cooker->Prepare();
			}
		}
	}

	if (Cooker.IsValid())
	{
		TMap<FName, FVector4> NewMaterialParams;

		Cooker->PackTexture(PackedTextureDefIndex, NewMaterialParams);

		if (!NewMaterialParams.IsEmpty())
		{
			FScopeLock Lock(&TextureSetCookCS);
			MaterialParameters.Append(NewMaterialParams);
		}
	}

	// Free temporary image raw data if all cooked textures have been cached
	if (CVarTextureSetFreeImmediateImages.GetValueOnAnyThread())
	{
		FScopeLock Lock(&TextureSetCookCS);

		int32 CookedTexturesMask = (1 << CookedTextures.Num()) - 1;
		if (CookedTexturesProcessedBitmask == CookedTexturesMask)
		{
			Cooker.Reset();
		}
	}
}

void UTextureSet::UpdateResource()
{
	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();

	for (int32 PackedTextureIndex = 0; PackedTextureIndex < PackingInfo.NumPackedTextures(); PackedTextureIndex++)
	{
		UTexture* CookedTexture = GetCookedTexture(PackedTextureIndex);
		CookedTexture->UpdateResource();
	}
}

void UTextureSet::CookImmediate(bool Force)
{
	check(!Cooker.IsValid()); // Don't want to mess up another cook in progress
	check(IsValid(Definition));

	FScopedSlowTask CookTask(1.0f, LOCTEXT("CookingTextureSet", "Cooking Texture Set..."));
	CookTask.MakeDialog();

	FOnTextureSetCookerReportProgress CookProgress = FOnTextureSetCookerReportProgress::CreateLambda([&CookTask](float f) { CookTask.EnterProgressFrame(f); });

	UpdateCookedTextures();
	TArray<FString> NewPackedTextureKeys = ComputePackedTextureKeys();
	if ((NewPackedTextureKeys != PackedTextureKeys) || Force)
	{
		PackedTextureKeys = NewPackedTextureKeys;
		MaterialParameters.Empty();

		Cooker = MakeUnique<TextureSetCooker>(this, CookProgress);
		Cooker->Prepare();
		Cooker->PackAllTextures(MaterialParameters);
		Cooker.Reset();

		for (int32 i = 0; i < CookedTextures.Num(); i++)
			CookedTextures[i]->Modify(true);

		UpdateResource();
	}
}

#undef LOCTEXT_NAMESPACE
