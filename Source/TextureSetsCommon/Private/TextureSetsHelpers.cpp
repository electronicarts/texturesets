// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetsHelpers.h"

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "TextureSetPackedTextureDef.h"
#include "TextureSetInfo.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

DEFINE_LOG_CATEGORY(LogTextureSet);

#if WITH_EDITOR
bool TextureSetsHelpers::GetSourceDataIdAsString(const UTexture* Texture, FString& StringOut)
{
	if (Texture->Source.IsValid() && !Texture->bSourceBulkDataTransient)
	{
		StringOut = Texture->Source.GetId().ToString();
		return true;
	}

	return false;
}
#endif

bool TextureSetsHelpers::GetSourceDataIdAsString(const FAssetData& AssetData, FString& StringOut)
{
	if (AssetData.IsValid())
	{
		const FAssetTagValueRef FoundValue = AssetData.TagsAndValues.FindTag(TextureBulkDataIdAssetTagName);
		if (FoundValue.IsSet())
		{
			StringOut = FoundValue.AsString();
			return true;
		}
	}

	return false;
}

TArray<FName> TextureSetsHelpers::GetUnpackedChannelNames(const TArray<FTextureSetPackedTextureDef>& PackedTextures, const TMap<FName, FTextureSetProcessedTextureDef>& ProcessedTextures)
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
			FName ChannelName = FName(Name.ToString() + TextureSetsHelpers::ChannelSuffixes[i]);

			if (!PackedNames.Contains(ChannelName))
				UnpackedNames.Add(ChannelName);
		}
	}

	return UnpackedNames;
}

IAssetRegistry& TextureSetsHelpers::GetAssetRegistry()
{
	return FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry")).Get();
}

TArray<FAssetData> TextureSetsHelpers::GetDependenciesForPackage(const FName& PackageName, TObjectPtr<const UClass> FilterClass)
{
	TArray<FAssetData> PackageDependencies;
	IAssetRegistry& AssetRegistry = GetAssetRegistry();
	
	TArray<FAssetIdentifier> Dependencies;
	AssetRegistry.GetReferencers(PackageName, Dependencies);
	for (FAssetIdentifier& Dependency : Dependencies)
	{
		TArray<FAssetData> DependencyAssetData;
		AssetRegistry.GetAssetsByPackageName(Dependency.PackageName, DependencyAssetData);
		PackageDependencies.Append(DependencyAssetData);
	}

	if (IsValid(FilterClass))
	{
		FARFilter Filter;
		Filter.bRecursiveClasses = true;
		Filter.ClassPaths.Add(FilterClass->GetClassPathName());
		AssetRegistry.RunAssetsThroughFilter(PackageDependencies, Filter);
	}
	
	return PackageDependencies;
}

TMap<TObjectPtr<UClass>, TArray<FAssetData>> TextureSetsHelpers::GetDependenciesForPackage(const FName& PackageName, TArrayView<const TObjectPtr<UClass>> FilterClasses)
{
	TMap<TObjectPtr<UClass>, TArray<FAssetData>> ClassToDependenciesMap;
	
	const TArray<FAssetData> PackageDependencies = GetDependenciesForPackage(PackageName);

	for (TObjectPtr<UClass> Class : FilterClasses)
	{
		TArray<FAssetData> FilteredDependencies = PackageDependencies;
		FARFilter Filter;
		Filter.bRecursiveClasses = true;
		Filter.ClassPaths.Add(Class->GetClassPathName());

		GetAssetRegistry().RunAssetsThroughFilter(FilteredDependencies, Filter);
		ClassToDependenciesMap.Add(Class, FilteredDependencies);
	}
	
	return ClassToDependenciesMap;
}

FName TextureSetsHelpers::MakeTextureParameterName(FName ParameterName, int TextureIndex)
{
	// To reduce the runtime cost of this method, an parameter FName cache is maintained, which avoids creating an FName from a string and formatting a string
	// Original code:
	// return FName(FString::Format(TEXT("TEXSET_{0}_PACKED_{1}"), {ParameterName.ToString(), FString::FromInt(TextureIndex)}));
	static TArray<TMap<FName, FName>> ParameterNameMap;

	check(IsInGameThread());

	// Look up parameter name + index in cache first
	if (ParameterNameMap.Num() > TextureIndex)
	{
		FName* CachedParameterName = ParameterNameMap[TextureIndex].Find(ParameterName);

		if (CachedParameterName)
		{
			return *CachedParameterName;
		}
	}

	// On cache miss, create a new FName for that ParameterName + TextureIndex combo
	check(TextureIndex >= 0 && TextureIndex < 100);
	check(ParameterName.GetStringLength() < 230);

	TCHAR TempNameBuffer[256] = TEXT("TEXSET_");
	TCHAR* Current = TempNameBuffer + 7;

	uint32 Size = ParameterName.ToString(Current, 256);
	Current += Size;

	const TCHAR kSuffix[] = TEXT("_PACKED_");
	FCString::Strcpy(Current, UE_ARRAY_COUNT(TempNameBuffer) - Size, kSuffix);
	Current += (UE_ARRAY_COUNT(kSuffix) - 1);

	if (TextureIndex >= 10)
	{
		int Tens = TextureIndex / 10;
		Current[0] = TEXT('0') + Tens;
		++Current;
	}
	Current[0] = TEXT('0') + TextureIndex % 10;
	Current[1] = 0;

	FName BuiltParameterName(TempNameBuffer);

	// Insert the newly built FName in the cache
	if (ParameterNameMap.Num() <= TextureIndex)
		ParameterNameMap.SetNum(FMath::Max(TextureIndex + 1, 32));

	ParameterNameMap[TextureIndex].Add(ParameterName, BuiltParameterName);

	return BuiltParameterName;
}

FName TextureSetsHelpers::MakeConstantParameterName(FName ParameterName, FName ConstantName)
{
	return FName(FString::Format(TEXT("TEXSET_{0}_{1}"), { ParameterName.ToString(), ConstantName.ToString() }));
}

bool TextureSetsHelpers::IsTextureSetParameterName(FName Name)
{
	return Name.ToString().StartsWith("TEXSET_", ESearchCase::IgnoreCase);
}

UReferenceHolder* FReferenceHolder::GetHolder()
{
	static UReferenceHolder* Holder = nullptr;

	if (!Holder)
	{
		Holder = NewObject<UReferenceHolder>();
		Holder->AddToRoot();
	}

	return Holder;
}
