// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetDefinition.h"

#include "TextureSet.h"
#include "TextureSetModule.h"
#include "TextureSetPackedTextureDef.h"
#include "UObject/ObjectSaveContext.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "ImageUtils.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

const FString UTextureSetDefinition::ChannelSuffixes[4] = {".r", ".g", ".b", ".a"};

UTextureSetDefinition::UTextureSetDefinition() : Super()
{
	if (!UniqueID.IsValid())
		UniqueID = FGuid::NewGuid();
}

#if WITH_EDITOR
EDataValidationResult UTextureSetDefinition::IsDataValid(FDataValidationContext& Context)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	// Module validation
	for (int i = 0; i < Modules.Num(); i++)
	{
		const UTextureSetModule* Module = Modules[i];
		if (!IsValid(Module))
		{
			Context.AddError(FText::Format(LOCTEXT("MissingModule","Module at index {0} is invalid."), i));
			Result = EDataValidationResult::Invalid;
		}
	}

	// Packing validation
	for (FName Name : GetUnpackedChannelNames(PackingInfo.PackedTextureDefs))
	{
		if (Name.IsNone())
			continue;

		Context.AddError(FText::Format(LOCTEXT("UnpackedChannel","\"{0}\" is unused, did you forget to pack it?"), FText::FromName(Name)));
		Result = EDataValidationResult::Invalid;
	}

	for (int i = 0; i < PackingInfo.NumPackedTextures(); i++)
	{
		const FTextureSetPackedTextureDef& PackedTexture = PackingInfo.GetPackedTextureDef(i);

		if (PackedTexture.UsedChannels() > PackedTexture.AvailableChannels())
		{
			Context.AddError(FText::Format(LOCTEXT("OverpackedTexture","Packed texture {0} is specifying more packed channels than are provided by the chosen compression format."), i));
			Result = EDataValidationResult::Invalid;
		}
	}

	return CombineDataValidationResults(Result, Super::IsDataValid(Context));;
}
#endif

#if WITH_EDITOR
bool UTextureSetDefinition::CanEditChange(const FProperty* InProperty) const
{
	// If other logic prevents editing, we want to respect that
	const bool SuperValue = Super::CanEditChange(InProperty);

	if (InProperty->GetOwnerStruct() == FTextureSetPackedTextureDef::StaticStruct())
	{
		const FTextureSetPackedTextureDef* TextureDef = nullptr; // TODO: Have to get this from the property somehow

		if (TextureDef)
		{
			if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FTextureSetPackedTextureDef, SourceR))
			{
				return SuperValue && TextureDef->AvailableChannels() > 0;
			}

			if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FTextureSetPackedTextureDef, SourceG))
			{
				return SuperValue && TextureDef->AvailableChannels() > 1;
			}

			if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FTextureSetPackedTextureDef, SourceB))
			{
				return SuperValue && TextureDef->AvailableChannels() > 2;
			}

			if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FTextureSetPackedTextureDef, SourceA))
			{
				return SuperValue && TextureDef->AvailableChannels() > 3;
			}
		}
	}

	return SuperValue;
}
#endif

#if WITH_EDITOR
void UTextureSetDefinition::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

	ApplyEdits();
}
#endif


void UTextureSetDefinition::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	ResetEdits();
	// Also re-apply edits, to account for any changes to module logic
	ApplyEdits();
#endif
}

#if WITH_EDITOR
TArray<FName> UTextureSetDefinition::EditGetUnpackedChannelNames() const
{
	return GetUnpackedChannelNames(EditPackedTextures);
}
#endif

TArray<FName> UTextureSetDefinition::GetUnpackedChannelNames(TArray<FTextureSetPackedTextureDef> Textures) const
{
	// Construct an array of all the channel names already used for packing
	TArray<FName> PackedNames = TArray<FName>();

	for (const FTextureSetPackedTextureDef& Tex : Textures)
	{
		if (!Tex.SourceR.IsNone())
			PackedNames.Add(Tex.SourceR);

		if (!Tex.SourceG.IsNone())
			PackedNames.Add(Tex.SourceG);

		if (!Tex.SourceB.IsNone())
			PackedNames.Add(Tex.SourceB);

		if (!Tex.SourceA.IsNone())
			PackedNames.Add(Tex.SourceA);
	}

	// Construct a list of channel names not yet used for packing (remaining choices)
	TArray<FName> UnpackedNames = TArray<FName>{  FName() };

	for (const FTextureSetProcessedTextureDef& Tex : GetModuleInfo().GetProcessedTextures())
	{
		for (int i = 0; i < Tex.ChannelCount; i++)
		{
			FName ChannelName = FName(Tex.Name.ToString() + ChannelSuffixes[i]);

			if (!PackedNames.Contains(ChannelName))
				UnpackedNames.Add(ChannelName);
		}
	}

	return UnpackedNames;
}

const FTextureSetDefinitionModuleInfo UTextureSetDefinition::GetModuleInfo() const
{
	return ModuleInfo;
}

const FTextureSetDefinitionSamplingInfo UTextureSetDefinition::GetSamplingInfo(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	FTextureSetDefinitionSamplingInfo Info;

	for (const UTextureSetModule* Module : GetModules())
	{
		if (IsValid(Module))
		{
			Module->BuildSamplingInfo(Info, SampleExpression);
		}
	}

	return Info;
}

const FTextureSetPackingInfo UTextureSetDefinition::GetPackingInfo() const
{
	return PackingInfo;
}

const TArray<const UTextureSetModule*> UTextureSetDefinition::GetModules() const
{
	TArray<const UTextureSetModule*> ValidModules;

	for (const UTextureSetModule* Module : Modules)
	{
		if (IsValid(Module))
			ValidModules.Add(Module);
	}

	return ValidModules;
}

TArray<TSubclassOf<UTextureSetAssetParams>> UTextureSetDefinition::GetRequiredAssetParamClasses() const
{
	TArray<TSubclassOf<UTextureSetAssetParams>> RequiredTypes;
	for (const UTextureSetModule* Module : Modules)
	{
		if (IsValid(Module))
		{
			UClass* SampleParamClass = Module->GetAssetParamClass();

			if (SampleParamClass != nullptr)
				if (!RequiredTypes.Contains(SampleParamClass))
					RequiredTypes.Add(SampleParamClass);
		}
	}
	return RequiredTypes;
}

TArray<TSubclassOf<UTextureSetSampleParams>> UTextureSetDefinition::GetRequiredSampleParamClasses() const
{
	TArray<TSubclassOf<UTextureSetSampleParams>> RequiredTypes;
	for (const UTextureSetModule* Module : GetModules())
	{
		UClass* SampleParamClass = Module->GetSampleParamClass();

		if (SampleParamClass != nullptr)
			if (!RequiredTypes.Contains(SampleParamClass))
				RequiredTypes.Add(SampleParamClass);
	}
	return RequiredTypes;
}

UTexture* UTextureSetDefinition::GetDefaultPackedTexture(int Index)
{
	return DefaultTextureSet->GetDerivedTexture(Index);
}

uint32 UTextureSetDefinition::ComputeCookingHash(int PackedTextureIndex)
{
	uint32 Hash = 0;

	TArray<TSubclassOf<UTextureSetSampleParams>> RequiredTypes;
	// TODO: Only hash modules that contribute to this packed texture
	for (const UTextureSetModule* Module : GetModules())
	{
		Hash = HashCombine(Hash, Module->ComputeProcessingHash());
	}

	Hash = HashCombine(Hash, GetTypeHash(PackingInfo.PackedTextureDefs[PackedTextureIndex]));

	return Hash;
}

uint32 UTextureSetDefinition::ComputeCookingHash()
{
	uint32 Hash = 0;

	for (int i = 0; i < PackingInfo.PackedTextureDefs.Num(); i++)
	{
		Hash = HashCombine(Hash, ComputeCookingHash(i));
	}

	return Hash;
}


uint32 UTextureSetDefinition::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression)
{
	uint32 Hash = 0;

	TArray<TSubclassOf<UTextureSetSampleParams>> RequiredTypes;
	for (const UTextureSetModule* Module : GetModules())
	{
		Hash = HashCombine(Hash, Module->ComputeSamplingHash(SampleExpression));
	}

	return Hash;
}

#if WITH_EDITOR
void UTextureSetDefinition::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	TArray<TSubclassOf<UTextureSetSampleParams>> RequiredTypes;
	for (const UTextureSetModule* Module : GetModules())
	{
		Module->GenerateSamplingGraph(SampleExpression, Builder);
	}
}
#endif

#if WITH_EDITOR
void UTextureSetDefinition::ResetEdits()
{
	// Modules
	EditModules.Empty();

	for (const UTextureSetModule* Module : Modules)
	{
		if (IsValid(Module))
			EditModules.Add(DuplicateObject(Module, this));
	}

	// Packing
	EditPackedTextures = PackingInfo.PackedTextureDefs;
}
#endif

#if WITH_EDITOR
void UTextureSetDefinition::ApplyEdits()
{
	// Update module info
	Modules.Empty();
	ModuleInfo = FTextureSetDefinitionModuleInfo();

	for (const UTextureSetModule* EditModule : EditModules)
	{
		if (IsValid(EditModule))
		{
			EditModule->BuildModuleInfo(ModuleInfo);
			Modules.Add(DuplicateObject(EditModule, this));
		}
	}

	// Update packing info
	PackingInfo = FTextureSetPackingInfo();

	for (int i = 0; i < EditPackedTextures.Num(); i++)
	{
		const FTextureSetPackedTextureDef& TextureDef = EditPackedTextures[i];
		FTextureSetPackedTextureInfo TextureInfo;

		TArray<FName> SourceNames = TextureDef.GetSources();
		TextureInfo.ChannelCount = SourceNames.Num();
		TextureInfo.HardwareSRGB = TextureDef.GetHardwareSRGBEnabled();

		for (int c = 0; c < TextureInfo.ChannelCount; c++)
		{
			if (SourceNames[c].IsNone())
				continue;

			FString SourceTexString;
			FString SourceChannelString;
			SourceNames[c].ToString().Split(".", &SourceTexString, &SourceChannelString);
			const FName SourceTexName = FName(SourceTexString);

			if (!ModuleInfo.HasProcessedTextureOfName(SourceTexName))
				continue;

			FTextureSetProcessedTextureDef Processed = ModuleInfo.GetProcessedTextureByName(SourceTexName);

			static const TMap<FString, int> ChannelStringLookup = {
				{"r", 0},
				{"g", 1},
				{"b", 2},
				{"a", 3},
			};

			const int SourceChannel = ChannelStringLookup.FindChecked(SourceChannelString);

			TextureInfo.ChannelInfo[c].ProcessedTexture = SourceTexName;
			TextureInfo.ChannelInfo[c].ProessedTextureChannel = SourceChannel;

			if (Processed.SRGB)
			{
				TextureInfo.ChannelInfo[c].ChannelEncoding = ETextureSetTextureChannelEncoding::SRGB;
			}
			else
			{
				if (c < 3)
					TextureInfo.HardwareSRGB = false; // If we have any non-sRGB textures in R, G, or B, then we can't use hardware SRGB.

				if (TextureDef.bDoRangeCompression)
					TextureInfo.ChannelInfo[c].ChannelEncoding = ETextureSetTextureChannelEncoding::Linear_RangeCompressed;
				else
					TextureInfo.ChannelInfo[c].ChannelEncoding = ETextureSetTextureChannelEncoding::Linear_Raw;
			}
		}

		TextureInfo.RangeCompressMulName = FName("RangeCompress_" + FString::FromInt(i) + "_Mul");
		TextureInfo.RangeCompressAddName = FName("RangeCompress_" + FString::FromInt(i) + "_Add");

		PackingInfo.PackedTextureDefs.Add(TextureDef);
		PackingInfo.PackedTextureInfos.Add(TextureInfo);
	}

	// Update default texture set to reflect the new data
	if (!IsValid(DefaultTextureSet))
	{
		FName DefaultName = FName(GetName() + "_Default");
		DefaultTextureSet = NewObject<UTextureSet>(this, DefaultName, RF_Transient);
		DefaultTextureSet->Definition = this;
	}

	DefaultTextureSet->UpdateDerivedData();

	const uint32 NewHash = ComputeCookingHash();

	if (NewHash != CookingHash)
	{
		CookingHash = NewHash;
		UpdateDependentAssets(true);
	}
}
#endif

#if WITH_EDITOR
void UTextureSetDefinition::UpdateDependentAssets(bool bCookingHashChanged)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetIdentifier> Referencers;
	AssetRegistry.GetReferencers(GetOuter()->GetFName(), Referencers);

	for (auto AssetIdentifier : Referencers)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(AssetIdentifier.PackageName, Assets);

		for (auto AssetData : Assets)
		{
			if (AssetData.IsInstanceOf(UTextureSet::StaticClass()))
			{
				UTextureSet* TextureSet = Cast<UTextureSet>(AssetData.FastGetAsset(false));
				if (TextureSet != nullptr && TextureSet->Definition == this)
				{
					TextureSet->UpdateDerivedData();
				}
			}
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
