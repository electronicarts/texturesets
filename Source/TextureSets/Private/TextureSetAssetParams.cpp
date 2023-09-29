// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetAssetParams.h"

void FTextureSetAssetParamsCollection::UpdateParamList(UObject* Outer, TArray<TSubclassOf<UTextureSetAssetParams>> RequiredParamClasses)
{
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
		}
	}

	// Add missing sample params
	for (TSubclassOf<UTextureSetAssetParams>& SampleParamClass : RequiredParamClasses)
	{
		ParamList.Add(NewObject<UTextureSetAssetParams>(Outer, SampleParamClass));
	}
}
