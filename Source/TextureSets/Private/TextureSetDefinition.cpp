// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetDefinition.h"

#include "Async/Async.h"
#include "TextureSet.h"
#include "TextureSetModule.h"
#include "TextureSetPackedTextureDef.h"
#include "TextureSetsHelpers.h"
#include "UObject/ObjectSaveContext.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "ProcessingNodes/IProcessingNode.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

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
	for (int i = 0; i < GetModuleInfo().GetModules().Num(); i++)
	{
		const UTextureSetModule* Module = GetModuleInfo().GetModules()[i];
		if (!IsValid(Module))
		{
			Context.AddError(FText::Format(LOCTEXT("MissingModule","Module at index {0} is invalid."), i));
			Result = EDataValidationResult::Invalid;
		}
	}

	// Packing validation
	for (int i = 0; i < PackingInfo.Warnings.Num(); i++)
	{
		Context.AddWarning(PackingInfo.Warnings[i]);
	}

	for (int i = 0; i < PackingInfo.Errors.Num(); i++)
	{
		Context.AddError(PackingInfo.Errors[i]);
		Result = EDataValidationResult::Invalid;
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

#if WITH_EDITOR
TArray<FName> UTextureSetDefinition::EditGetUnpackedChannelNames() const
{
	FTextureSetProcessingGraph ProcessingGraph = FTextureSetProcessingGraph(TArray<const UTextureSetModule*>(EditModules));
	TMap<FName, FTextureSetProcessedTextureDef> ProcessedTextures;
	for (const auto&[Name, Output] : ProcessingGraph.GetOutputTextures())
	{
		ProcessedTextures.Add(Name, Output->GetTextureDef());
	}

	return TextureSetsHelpers::GetUnpackedChannelNames(EditPackedTextures, ProcessedTextures);
}
#endif

TArray<TSubclassOf<UTextureSetAssetParams>> UTextureSetDefinition::GetRequiredAssetParamClasses() const
{
	TArray<TSubclassOf<UTextureSetAssetParams>> RequiredTypes;
	for (const UTextureSetModule* Module : ModuleInfo.GetModules())
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
	for (const UTextureSetModule* Module : GetModuleInfo().GetModules())
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

	const FTextureSetProcessingGraph* Graph = GetModuleInfo().GetProcessingGraph();

	for (const auto& [Name, Node] : Graph->GetOutputTextures())
		Hash = HashCombine(Hash, Node->ComputeGraphHash());

	for (int i = 0; i < PackingInfo.PackedTextureDefs.Num(); i++)
		Hash = HashCombine(Hash, GetTypeHash(PackingInfo.PackedTextureDefs[i]));

	for (const auto& [Name, Node] : Graph->GetOutputParameters())
		Hash = HashCombine(Hash, Node->ComputeGraphHash());

	return Hash;
}
#endif

#if WITH_EDITOR
void UTextureSetDefinition::ResetEdits()
{
	// Modules
	EditModules.Empty();

	for (const UTextureSetModule* Module : ModuleInfo.GetModules())
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
	// Duplicate edit modules
	TArray<const UTextureSetModule*> Modules;

	for (const UTextureSetModule* EditModule : EditModules)
	{
		if (IsValid(EditModule))
			Modules.Add(DuplicateObject(EditModule, this));
	}
	
	// Update module info
	ModuleInfo = FTextureSetDefinitionModuleInfo(Modules);

	// Update packing info
	PackingInfo = FTextureSetPackingInfo(EditPackedTextures, ModuleInfo);

	const uint32 NewHash = ComputeCookingHash();

	// Ensure texture set always has up to date derived data available immediately
	DefaultTextureSet->UpdateDerivedData();

	if (NewHash != CookingHash)
	{
		CookingHash = NewHash;

		// Broadcast with an async event so we get called from a consistent place in the main thread.
		// (Not during load, save, etc.)
		AsyncTask(ENamedThreads::GameThread, [this] ()
		{
			FOnTextureSetDefinitionChangedEvent.Broadcast(this);
		});
	}
}
#endif

#undef LOCTEXT_NAMESPACE
