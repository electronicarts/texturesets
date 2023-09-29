// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetDefinition.h"

#include "TextureSet.h"
#include "TextureSetModule.h"
#include "TextureSetPackedTextureDef.h"
#include "UObject/ObjectSaveContext.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

const TArray<FString> UTextureSetDefinition::ChannelSuffixes = {".r", ".g", ".b", ".a"};

#if WITH_EDITOR
FOnTextureSetDefinitionChanged UTextureSetDefinition::FOnTextureSetDefinitionChangedEvent;
#endif

UTextureSetDefinition::UTextureSetDefinition() : Super()
{
	if (!UniqueID.IsValid())
		UniqueID = FGuid::NewGuid();

	DefaultTextureSet = CreateDefaultSubobject<UTextureSet>(FName(GetName() + "_Default"));
	DefaultTextureSet->Definition = this;
	DefaultTextureSet->SetFlags(RF_Public);
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
	for (FName Name : GetUnpackedChannelNames())
	{
		if (Name.IsNone())
			continue;

		Context.AddError(FText::Format(LOCTEXT("UnpackedChannel","\"{0}\" is unused, did you forget to pack it?"), FText::FromName(Name)));
		Result = EDataValidationResult::Invalid;
	}

	TArray<FName> PackedChannels;

	for (int i = 0; i < PackingInfo.NumPackedTextures(); i++)
	{
		const FTextureSetPackedTextureDef& PackedTexture = PackingInfo.GetPackedTextureDef(i);

		if (PackedTexture.GetUsedChannels() > PackedTexture.GetAvailableChannels())
		{
			Context.AddError(FText::Format(LOCTEXT("OverpackedTexture","Packed texture {0} is specifying more packed channels than are provided by the chosen compression format."), i));
			Result = EDataValidationResult::Invalid;
		}

		for (FName PackedChannel : PackedTexture.GetSources())
		{
			if (PackedChannels.Contains(PackedChannel))
			{
				Context.AddError(FText::Format(LOCTEXT("ChannelDuplicated","Packed texture channel {0} appears more than once in the packing definition."), FText::FromName(PackedChannel)));
				Result = EDataValidationResult::Invalid;
			}
			else
			{
				PackedChannels.Add(PackedChannel);
			}
		}
	}

	return CombineDataValidationResults(Result, Super::IsDataValid(Context));
}
#endif

#if WITH_EDITOR
void UTextureSetDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FTextureSetPackedTextureDef, CompressionSettings) ||
		PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UTextureSetDefinition, EditPackedTextures))
	{
		for (FTextureSetPackedTextureDef& PackedTextureDef : EditPackedTextures)
		{
			PackedTextureDef.UpdateAvailableChannels();
		}
	}
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

static TArray<FName> StaticGetUnpackedChannelNames(const TArray<FTextureSetPackedTextureDef>& PackedTextures, const TMap<FName, FTextureSetProcessedTextureDef>& ProcessedTextures)
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

	for (const auto& [Name, Tex] : ProcessedTextures)
	{
		for (int i = 0; i < Tex.ChannelCount; i++)
		{
			FName ChannelName = FName(Name.ToString() + UTextureSetDefinition::ChannelSuffixes[i]);

			if (!PackedNames.Contains(ChannelName))
				UnpackedNames.Add(ChannelName);
		}
	}

	return UnpackedNames;
}

#if WITH_EDITOR
TArray<FName> UTextureSetDefinition::EditGetUnpackedChannelNames() const
{
	FTextureSetProcessingGraph ProcessingGraph = FTextureSetProcessingGraph(TArray<const UTextureSetModule*>(EditModules));
	TMap<FName, FTextureSetProcessedTextureDef> ProcessedTextures;
	for (const auto&[Name, Output] : ProcessingGraph.GetOutputTextures())
	{
		ProcessedTextures.Add(Name, Output->GetTextureDef());
	}

	return StaticGetUnpackedChannelNames(EditPackedTextures, ProcessedTextures);
}
#endif

TArray<FName> UTextureSetDefinition::GetUnpackedChannelNames() const
{
	return StaticGetUnpackedChannelNames(GetPackingInfo().PackedTextureDefs, GetModuleInfo().GetProcessedTextures());
}

const FTextureSetDefinitionModuleInfo& UTextureSetDefinition::GetModuleInfo() const
{
	return ModuleInfo;
}

const FTextureSetPackingInfo& UTextureSetDefinition::GetPackingInfo() const
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

const UTextureSet* UTextureSetDefinition::GetDefaultTextureSet() const
{
	check(IsValid(DefaultTextureSet));
	return DefaultTextureSet;
}

#if WITH_EDITOR
uint32 UTextureSetDefinition::ComputeCookingHash()
{
	uint32 Hash = GetTypeHash(FString("TextureSetDefinition"));

	// Key for debugging, easily force rebuild
	Hash = HashCombine(Hash, GetTypeHash(UserKey));

	FTextureSetProcessingGraph Graph = FTextureSetProcessingGraph(GetModules());

	for (const auto& [Name, Node] : Graph.GetOutputTextures())
		Hash = HashCombine(Hash, Node->ComputeGraphHash());

	for (int i = 0; i < PackingInfo.PackedTextureDefs.Num(); i++)
		Hash = HashCombine(Hash, GetTypeHash(PackingInfo.PackedTextureDefs[i]));

	for (const auto& [Name, Node] : Graph.GetOutputParameters())
		Hash = HashCombine(Hash, Node->ComputeGraphHash());

	return Hash;
}
#endif

#if WITH_EDITOR
void UTextureSetDefinition::ResetEdits()
{
	// Modules
	EditModules.Empty();

	for (const UTextureSetModule* Module : Modules)
	{
		// Edit modules have a null outer so they will not be serialized
		// Having 'this' as an outer caused a crash in RoutePresave (SavePackage2.cpp)
		// when it was included in the save, and then cleaned up by the GC mid-save.
		if (IsValid(Module))
			EditModules.Add(DuplicateObject(Module, nullptr));
	}

	// Packing
	EditPackedTextures = PackingInfo.PackedTextureDefs;

	for (FTextureSetPackedTextureDef& PackedTextureDef : EditPackedTextures)
	{
		PackedTextureDef.UpdateAvailableChannels();
	}
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
			Modules.Add(DuplicateObject(EditModule, this));
		}
	}

	FTextureSetProcessingGraph ProcessingGraph = FTextureSetProcessingGraph(Modules);

	for (const auto&[Name, Input] : ProcessingGraph.GetInputTextures())
	{
		ModuleInfo.SourceTextures.Add(Name, Input->GetTextureDef());
	}

	for (const auto&[Name, Output] : ProcessingGraph.GetOutputTextures())
	{
		ModuleInfo.ProcessedTextures.Add(Name, Output->GetTextureDef());
	}

	// Update packing info
	PackingInfo = FTextureSetPackingInfo();

	for (int i = 0; i < EditPackedTextures.Num(); i++)
	{
		FTextureSetPackedTextureDef& TextureDef = EditPackedTextures[i];
		FTextureSetPackedTextureInfo TextureInfo;

		// Enabled initially, but can be disabled if it's found to be incompatible with the current packing
		TextureInfo.HardwareSRGB = TextureDef.GetHardwareSRGBEnabled();

		for (int c = 0; c < TextureDef.GetSources().Num(); c++)
		{
			FName Source = TextureDef.GetSources()[c];

			if (Source.IsNone())
				continue;

			FString SourceTexString;
			FString SourceChannelString;
			Source.ToString().Split(".", &SourceTexString, &SourceChannelString);
			const FName SourceTexName = FName(SourceTexString);

			if (!ModuleInfo.GetProcessedTextures().Contains(SourceTexName))
			{
				// Trying to pack a channel from a texture that doesn't exist
				TextureDef.SetSource(c, FName());
				continue;
			}

			const int SourceChannel = ChannelSuffixes.Find("." + SourceChannelString);
			check (SourceChannel != INDEX_NONE);

			FTextureSetProcessedTextureDef Processed = ModuleInfo.GetProcessedTextures().FindChecked(SourceTexName);

			if (SourceChannel >= Processed.ChannelCount)
			{
				// Trying to pack a channel that doesn't exist in the processed texture output
				TextureDef.SetSource(c, FName());
				continue;
			}

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

		// Set channel count after loop, incase any channels were removed
		TextureInfo.ChannelCount = TextureDef.GetSources().Num();
		TextureInfo.RangeCompressMulName = FName("RangeCompress_" + FString::FromInt(i) + "_Mul");
		TextureInfo.RangeCompressAddName = FName("RangeCompress_" + FString::FromInt(i) + "_Add");

		PackingInfo.PackedTextureDefs.Add(TextureDef);
		PackingInfo.PackedTextureInfos.Add(TextureInfo);
	}

	const uint32 NewHash = ComputeCookingHash();

	// Ensure texture set always has up to date derived data available immediately
	DefaultTextureSet->UpdateDerivedData();

	if (NewHash != CookingHash)
	{
		CookingHash = NewHash;
		FOnTextureSetDefinitionChangedEvent.Broadcast(this);
	}
}
#endif

#undef LOCTEXT_NAMESPACE
