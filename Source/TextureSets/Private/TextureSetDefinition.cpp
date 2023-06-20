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

const FString UTextureSetDefinition::ChannelSuffixes[4] = {".r", ".g", ".b", ".a"};

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
void UTextureSetDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateDefaultTextures();
}
#endif

void UTextureSetDefinition::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	UpdateDefaultTextures();
#endif
}

TArray<FName> UTextureSetDefinition::GetUnpackedChannelNames() const
{
	// Construct an array of all the channel names already used for packing
	TArray<FName> PackedNames = TArray<FName>();

	for (const FTextureSetPackedTextureDef& Tex : PackedTextures)
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

	for (const TextureSetProcessedTextureDef& Tex : GetSharedInfo().GetProcessedTextures())
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

const TextureSetDefinitionSharedInfo UTextureSetDefinition::GetSharedInfo() const
{
	TextureSetDefinitionSharedInfo Info;

	for (UTextureSetModule* Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->BuildSharedInfo(Info);
		}
	}

	return Info;
}

const TextureSetDefinitionSamplingInfo UTextureSetDefinition::GetSamplingInfo(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	TextureSetDefinitionSamplingInfo Info;

	for (UTextureSetModule* Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->BuildSamplingInfo(Info, SampleExpression);
		}
	}

	return Info;
}

const TextureSetPackingInfo UTextureSetDefinition::GetPackingInfo() const
{
	TextureSetPackingInfo PackingInfo;
	TextureSetDefinitionSharedInfo SharedInfo = GetSharedInfo();

	for (int i = 0; i < PackedTextures.Num(); i++)
	{
		const FTextureSetPackedTextureDef& TextureDef = PackedTextures[i];
		TextureSetPackingInfo::TextureSetPackedTextureInfo TextureInfo;
		TextureInfo.AllowHardwareSRGB = true;

		TArray<FName> SourceNames = TextureDef.GetSources();
		TextureInfo.ChannelCount = SourceNames.Num();

		for (int c = 0; c < TextureInfo.ChannelCount; c++)
		{
			if (SourceNames[c].IsNone())
				continue;

			FString SourceTexString;
			FString SourceChannelString;
			SourceNames[c].ToString().Split(".", &SourceTexString, &SourceChannelString);
			const FName SourceTexName = FName(SourceTexString);

			static const TMap<FString, int> ChannelStringLookup = {
				{"r", 0},
				{"g", 1},
				{"b", 2},
				{"a", 3},
			};

			const int SourceChannel = ChannelStringLookup.FindChecked(SourceChannelString);

			TextureSetProcessedTextureDef Processed = SharedInfo.GetProcessedTextureByName(FName(SourceTexName));

			TextureInfo.ChannelInfo[c].ProcessedTexture = SourceTexName;
			TextureInfo.ChannelInfo[c].ProessedTextureChannel = SourceChannel;
			if (!Processed.SRGB && c < 3)
				TextureInfo.AllowHardwareSRGB = false; // If we have any non-sRGB textures in R, G, or B, then we can't use hardware SRGB.
		}

		TextureInfo.RangeCompressMulName = FName("RangeCompress_" + FString::FromInt(i) + "_Mul");
		TextureInfo.RangeCompressAddName = FName("RangeCompress_" + FString::FromInt(i) + "_Add");

		PackingInfo.PackedTextureDefs.Add(TextureDef);
		PackingInfo.PackedTextureInfos.Add(TextureInfo);
	}

	return PackingInfo;
}

const TArray<const UTextureSetModule*> UTextureSetDefinition::GetModules() const
{
	return TArray<const UTextureSetModule*>(Modules);
}

TArray<TSubclassOf<UTextureSetAssetParams>> UTextureSetDefinition::GetRequiredAssetParamClasses() const
{
	TArray<TSubclassOf<UTextureSetAssetParams>> RequiredTypes;
	for (UTextureSetModule* Module : Modules)
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
	for (UTextureSetModule* Module : Modules)
	{
		if (IsValid(Module))
		{
			UClass* SampleParamClass = Module->GetSampleParamClass();

			if (SampleParamClass != nullptr)
				if (!RequiredTypes.Contains(SampleParamClass))
					RequiredTypes.Add(SampleParamClass);
		}
	}
	return RequiredTypes;
}

#if WITH_EDITOR
void UTextureSetDefinition::UpdateDefaultTextures()
{
	TextureSetPackingInfo PackingInfo = GetPackingInfo();

	if (!IsValid(DefaultTextureSet))
	{
		FName DefaultName = FName(GetName() + "_Default");
		DefaultTextureSet = NewObject<UTextureSet>(this, DefaultName, RF_Transient);
		DefaultTextureSet->Definition = this;
	}

	DefaultTextureSet->UpdateTextureData();
}
#endif

UTexture* UTextureSetDefinition::GetDefaultPackedTexture(int Index)
{
#if WITH_EDITOR
	UpdateDefaultTextures();
#endif
	return DefaultTextureSet->GetPackedTexture(Index);
}

int32 UTextureSetDefinition::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression)
{
	uint32 Hash = 0;

	TArray<TSubclassOf<UTextureSetSampleParams>> RequiredTypes;
	for (UTextureSetModule* Module : Modules)
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
	for (UTextureSetModule* Module : Modules)
	{
		Module->GenerateSamplingGraph(SampleExpression, Builder);
	}
}
#endif

void UTextureSetDefinition::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
	UpdateDependentAssets(false);
}

void UTextureSetDefinition::UpdateDependentAssets(bool AutoLoad)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetIdentifier> Referencers;
	AssetRegistry.GetReferencers(GetOuter()->GetFName(), Referencers);

	TArray<FAssetData> TextureSetAssetDatas;

	for (auto AssetIdentifier : Referencers)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(AssetIdentifier.PackageName, Assets);

		for (auto AssetData : Assets)
		{
			if (AssetData.IsInstanceOf(UTextureSet::StaticClass()))
			{
				TextureSetAssetDatas.AddUnique(AssetData);
			}
		}
	}

	for (auto AssetData : TextureSetAssetDatas)
	{
		UTextureSet* TextureSet = Cast<UTextureSet>(AssetData.FastGetAsset(AutoLoad));
		if (TextureSet != nullptr)
		{
			// update texture set and create/destroy cooked textures
			TextureSet->UpdateTextureData();
			// mark texture set as mordified
			TextureSet->Modify();
			// broadcast the changes to other editor tabs
			TextureSet->PostEditChange();
			TextureSet->UpdateResource();
		}
	}
}