#include "TextureSetsAssetTypeActionsCommon.h"

#include "AssetRegistry/AssetRegistryModule.h"

namespace TextureSetsAssetTypeActionsCommon
{
	void GetReferencersData(UObject* Object, UClass* MatchClass, TArray<FAssetData>& OutAssetDatas)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		TArray<FAssetIdentifier> Referencers;
		AssetRegistry.GetReferencers(Object->GetOuter()->GetFName(), Referencers);

		for (auto AssetIdentifier : Referencers)
		{
			TArray<FAssetData> Assets;
			AssetRegistry.GetAssetsByPackageName(AssetIdentifier.PackageName, Assets);

			for (auto AssetData : Assets)
			{
				if (MatchClass != nullptr)
				{
					if (AssetData.IsInstanceOf(MatchClass))
					{
						OutAssetDatas.AddUnique(AssetData);
					}
				}
				else
				{
					OutAssetDatas.AddUnique(AssetData);
				}
			}
		}
	}
}
