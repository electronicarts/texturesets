// (c) Electronic Arts. All Rights Reserved.

#include "ElementCollection/TextureSetElementCollection.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "ElementCollection/TextureSetElementCollectionFactory.h"
#include "Engine/AssetManager.h"
#include "ProcessingNodes/TextureInput.h"
#include "TextureSetDefinition.h"
#include "TextureSetMaterialGraphBuilder.h"
#include "TextureSetProcessingGraph.h"

FString UTextureSetElementCollectionModule::GetInstanceName() const {
  if (IsValid(Collection))
    return Collection->GetName();
  else
    return Super::GetInstanceName();
}

void UTextureSetElementCollectionModule::ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	if (IsValid(Collection))
	{
		// Just pass through source texture as processed texture
		for (const auto& [ElementName, ElementDef]: Collection->Elements)
		{
			Graph.AddOutputTexture(ElementName, Graph.AddInputTexture(ElementName, ElementDef));
		}
	}
}

int32 UTextureSetElementCollectionModule::ComputeSamplingHash(const FTextureSetAssetParamsCollection* SampleParams) const
{
	uint32 Hash = Super::ComputeSamplingHash(SampleParams);

	if (IsValid(Collection))
	{
		for (const auto& [ElementName, ElementDef]: Collection->Elements)
		{
			Hash = HashCombine(Hash, GetTypeHash(ElementName.ToString()));
			Hash = HashCombine(Hash, GetTypeHash(ElementDef));
		}
	}
	
	return Hash;
}

void UTextureSetElementCollectionModule::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
	FTextureSetMaterialGraphBuilder* Builder) const
{
	if (IsValid(Collection))
	{
		Builder->AddSampleBuilder(SampleBuilderFunction([this, Builder](FTextureSetSubsampleContext& SampleContext)
		{
			// Create a sample result for each texture
			for (const auto& [ElementName, ElementDef]: Collection->Elements)
			{
				SampleContext.AddResult(ElementName, SampleContext.GetProcessedTextureSample(ElementName));
			}
		}));
	}
}

UTextureSetModule* UDEPRECATED_TextureSetElementCollection::DuplicateModule(UObject* Outer) const
{
	if (Outer == nullptr)
		Outer = GetTransientPackage();

	// Hacky, but upgrade data to the new format.
	// This can be removed when all data has been updated.

	const IAssetRegistry& AssetRegistry = UAssetManager::Get().GetAssetRegistry();
	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	FString AssetName;
	FString PackagePath;
	GetClass()->GetOuterUPackage()->GetName().Split("/", &PackagePath, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	AssetName += "_Asset";

	UTextureSetElementCollectionModule* Module = NewObject<UTextureSetElementCollectionModule>(Outer);
	
	FAssetData ExistingAsset = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(PackagePath + "/" + AssetName + "." + AssetName), false);
	UObject* ExistingAssetObject = ExistingAsset.GetAsset();
	if (IsValid(ExistingAssetObject))
	{
		Module->Collection = CastChecked<UTextureSetElementCollectionAsset>(ExistingAssetObject);
	}
	else
	{
		// Create a new asset by copying all the elements from the deprecated class.
		UTextureSetElementCollectionFactory* Factory = NewObject<UTextureSetElementCollectionFactory>();
		UTextureSetElementCollectionAsset* Asset = Cast<UTextureSetElementCollectionAsset>(AssetTools.CreateAsset(AssetName, PackagePath, Factory->SupportedClass, Factory));
		Asset->MarkPackageDirty();
		for (const FElementDefinition& Element : Elements)
		{
			Asset->Elements.Add(Element.ElementName, Element.ElementDef);
		}
		
		Module->Collection = Asset;
	}

	return Module;
}
