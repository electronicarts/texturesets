// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetAssetParams.h"

#if WITH_EDITOR
FOnTextureSetAssetParamsCollectionChanged FTextureSetAssetParamsCollection::OnCollectionChangedDelegate;

void FTextureSetAssetParamsCollection::UpdateParamList(UObject* Outer, TArray<TSubclassOf<UTextureSetAssetParams>> RequiredParamClasses)
{
	bool bListChanged = false;

	// Remove un-needed sample params
	for (int i = 0; i < ParamList.Num(); i++)
	{
		if (IsValid(ParamList[i]) && RequiredParamClasses.Contains(ParamList[i]->GetClass()))
		{
			RequiredParamClasses.Remove(ParamList[i]->GetClass()); // Remove this sample param from required array, so duplicates will be removed
		}
		else
		{
			ParamList.RemoveAt(i);
			i--;
			bListChanged = true;
		}
	}

	// Add missing sample params
	for (TSubclassOf<UTextureSetAssetParams>& SampleParamClass : RequiredParamClasses)
	{
		ParamList.Add(NewObject<UTextureSetAssetParams>(Outer, SampleParamClass));
		bListChanged = true;
	}

	if (bListChanged)
		OnCollectionChangedDelegate.Broadcast();
}
#endif
