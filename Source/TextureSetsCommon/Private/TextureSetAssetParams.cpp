// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetAssetParams.h"
#include "Templates/SubclassOf.h"
#include "UObject/UObjectGlobals.h"

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

			if (ParamList[i]->GetOuter() != Outer) // Mostly happens when a node is duplicated
			{
				// Replace the entry with a clone
				ParamList[i] = DuplicateObject<UTextureSetAssetParams>(ParamList[i], Outer);
			}
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

	for (UTextureSetAssetParams* Params : ParamList)
	{
		Params->FixupData(Outer);
	}

	if (bListChanged)
		OnCollectionChangedDelegate.Broadcast();
}
#endif
