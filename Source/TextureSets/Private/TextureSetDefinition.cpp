// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetDefinition.h"

#include "Async/Async.h"
#include "TextureSet.h"
#include "TextureSetModule.h"
#include "TextureSetPackedTextureDef.h"
#include "TextureSetsHelpers.h"
#include "UObject/ObjectSaveContext.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Misc/DataValidation.h"
#include "ProcessingNodes/IProcessingNode.h"
#include "ProcessingNodes/TextureInput.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

#if WITH_EDITOR
FOnTextureSetDefinitionChanged UTextureSetDefinition::FOnTextureSetDefinitionChangedEvent;
#endif

UTextureSetDefinition::UTextureSetDefinition() : Super()
#if WITH_EDITOR
	, ProcessingGraph(new FTextureSetProcessingGraph())
#endif
{
	if (!UniqueID.IsValid())
		UniqueID = FGuid::NewGuid();
}

#if WITH_EDITOR
EDataValidationResult UTextureSetDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	// Packing validation
	for (const FText& Error : PackingInfo.Errors)
	{
		Context.AddError(Error);
		Result = EDataValidationResult::Invalid;
	}

	// Module validation
	const TArray<const UTextureSetModule*>& Modules = GetModuleInfo().GetModules();

	for (int i = 0; i < Modules.Num(); i++)
	{
		const UTextureSetModule* Module = Modules[i];
		if (!IsValid(Module))
		{
			Context.AddError(FText::Format(LOCTEXT("MissingModule","Module at index {0} is invalid."), i));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			// Check for multiple modules with the same instance name
			for (int j = i + 1; j < Modules.Num(); j++)
			{
				const UTextureSetModule* OtherModule = Modules[j];

				if (Module->GetInstanceName() == OtherModule->GetInstanceName())
				{
					Context.AddError(FText::Format(LOCTEXT("MultipleModules","Module {0} at index {1} has the same instance name as module at index {2}."),
						FText::FromString(Module->GetInstanceName()),
						i,
						j));
					Result = EDataValidationResult::Invalid;

					break;
				}
			}

			if (Result != EDataValidationResult::Invalid)
			{
				// Allow module to run its own checks
				Result = CombineDataValidationResults(Result, Module->IsDefinitionValid(this, Context));
			}
		}
	}

	// Check processing graph for errors
	for (const FText& Error : ProcessingGraph->GetErrors())
	{
		Context.AddError(Error);
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

void UTextureSetDefinition::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.IsLoading())
	{
		// Packing source is not serialized, so if it's empty, re-create the packing info.
		if (PackingInfo.PackingSource.IsEmpty() && !PackingInfo.PackedTextureDefs.IsEmpty())
			PackingInfo = FTextureSetPackingInfo(PackingInfo.PackedTextureDefs, ModuleInfo);
	}
#endif
}

#if WITH_EDITOR
TArray<FName> UTextureSetDefinition::EditGetUnpackedChannelNames() const
{
	FTextureSetProcessingGraph TempProcessingGraph = FTextureSetProcessingGraph(TArray<const UTextureSetModule*>(EditModules));
	TMap<FName, FTextureSetProcessedTextureDef> ProcessedTextures;
	for (const auto&[Name, Output] : TempProcessingGraph.GetOutputTextures())
	{
		ProcessedTextures.Add(Name, Output->GetTextureDef());
	}

	return TextureSetsHelpers::GetUnpackedChannelNames(EditPackedTextures, ProcessedTextures);
}
#endif

#if WITH_EDITOR
TArray<TSubclassOf<UTextureSetAssetParams>> UTextureSetDefinition::GetRequiredAssetParamClasses() const
{
	TSet<TSubclassOf<UTextureSetAssetParams>> RequiredTypes;
	for (const UTextureSetModule* Module : ModuleInfo.GetModules())
	{
		if (IsValid(Module))
		{
			Module->GetAssetParamClasses(RequiredTypes);
		}
	}
	return RequiredTypes.Array();
}
#endif

#if WITH_EDITOR
TArray<TSubclassOf<UTextureSetSampleParams>> UTextureSetDefinition::GetRequiredSampleParamClasses() const
{
	TSet<TSubclassOf<UTextureSetSampleParams>> RequiredTypes;
	for (const UTextureSetModule* Module : GetModuleInfo().GetModules())
	{
		if (IsValid(Module))
		{
			Module->GetSampleParamClasses(RequiredTypes);
		}
	}
	return RequiredTypes.Array();
}
#endif

const UTextureSet* UTextureSetDefinition::GetDefaultTextureSet() const
{
	check(IsValid(DefaultTextureSet));
	return DefaultTextureSet;
}

#if WITH_EDITOR
uint32 UTextureSetDefinition::ComputeCompilationHash()
{
	uint32 Hash = GetTypeHash(FString("TextureSetDefinition"));

	// Key for debugging, easily force rebuild
	Hash = HashCombine(Hash, GetTypeHash(UserKey));

	for (const auto& [Name, Node] : ProcessingGraph->GetOutputTextures())
		Hash = HashCombine(Hash, Node->ComputeGraphHash());

	for (int i = 0; i < PackingInfo.PackedTextureDefs.Num(); i++)
		Hash = HashCombine(Hash, GetTypeHash(PackingInfo.PackedTextureDefs[i]));

	for (const auto& [Name, Node] : ProcessingGraph->GetOutputParameters())
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
			EditModules.Add(Module->DuplicateModule(nullptr));
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
			Modules.Add(EditModule->DuplicateModule(this));
	}
	
	// Update cached processing graph
	ProcessingGraph->Regenerate(Modules);

	// Update module info
	ModuleInfo = CreateModuleInfo(Modules, ProcessingGraph);

	// Update packing info
	PackingInfo = FTextureSetPackingInfo(EditPackedTextures, ModuleInfo);

	// Update the default Texture Set
	if (!IsValid(DefaultTextureSet))
		DefaultTextureSet = NewObject<UTextureSet>(this);

	DefaultTextureSet->Rename(*(GetName() + "_Default"), this);
	DefaultTextureSet->Definition = this;
	DefaultTextureSet->SetFlags(RF_Public);

	// Ensure default texture set always has up to date derived data available immediately
	DefaultTextureSet->UpdateDerivedData(false);

	const uint32 NewHash = ComputeCompilationHash();
	if (NewHash != CompilationHash)
	{
		CompilationHash = NewHash;

		// Force all material graphs with a sample param referencing us to be loaded, so they will receive the notifications and regenerate if needed.
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

			TArray<FAssetDependency> Referencers;
			AssetRegistry.GetReferencers(GetPackage()->GetFName(), Referencers);

			TArray<UClass*> SearchClases = {
				UMaterialFunction::StaticClass(),
				UMaterial::StaticClass()
			};

			for (const FAssetDependency& Dep : Referencers)
			{
				TArray<FAssetData> AssetsInPackage;
				AssetRegistry.GetAssetsByPackageName(Dep.AssetId.PackageName, AssetsInPackage, false);

				for (const FAssetData& AssetData : AssetsInPackage)
				{
					if (AssetData.GetClass()->GetDefaultObject()->IsA<UMaterial>() || AssetData.GetClass()->GetDefaultObject()->IsA<UMaterialFunction>())
					{
						AssetData.GetAsset();
					}
				}
			}
		}

		// Broadcast with an async event so we get called from a consistent place in the main thread.
		// (Not during load, save, etc.)
		AsyncTask(ENamedThreads::GameThread, [this] ()
		{
			FOnTextureSetDefinitionChangedEvent.Broadcast(this);
		});
	}
}
#endif

#if WITH_EDITOR
FTextureSetDefinitionModuleInfo UTextureSetDefinition::CreateModuleInfo(const TArray<const UTextureSetModule*>& Modules, const TSharedRef<FTextureSetProcessingGraph> ProcessingGraph)
{
	FTextureSetDefinitionModuleInfo Info;

	Info.Modules = Modules;

	for (const auto&[Name, Input] : ProcessingGraph->GetInputTextures())
	{
		Info.SourceTextures.Add(Name, Input->GetTextureDef());
	}

	for (const auto&[Name, Output] : ProcessingGraph->GetOutputTextures())
	{
		Info.ProcessedTextures.Add(Name, Output->GetTextureDef());
	}

	return Info;
}
#endif

#undef LOCTEXT_NAMESPACE
