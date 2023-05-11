#pragma once

namespace TextureSetsAssetTypeActionsCommon
{
	void GetReferencersData(UObject* Object, UClass* MatchClass, TArray<FAssetData>& OutAssetDatas);

	template <class T>
	void GetReferencersOfType(UObject* Object, TArray<TObjectPtr<T>>& OutObjects)
	{
		TArray<FAssetData> AssetDatas;
		GetReferencersData(Object, T::StaticClass(), AssetDatas);

		for (auto Data : AssetDatas)
		{
			T* TypedAsset = Cast<T>(Data.GetAsset());
			if (TypedAsset != nullptr)
			{
				OutObjects.Add(TypedAsset);
			}
		}
	}
};
