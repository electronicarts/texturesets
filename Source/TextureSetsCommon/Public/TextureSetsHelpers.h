// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IAssetRegistry;
class UTexture;
struct FAssetData;
struct FTextureSetPackedTextureDef;
struct FTextureSetProcessedTextureDef;

TEXTURESETSCOMMON_API DECLARE_LOG_CATEGORY_EXTERN(LogTextureSet, Log, All);

namespace TextureSetsHelpers
{
	// Suffixes used when addressing specific channels of a texture by name
	const TArray<FString> ChannelSuffixes = {".r", ".g", ".b", ".a"};

	const FName TextureBulkDataIdAssetTagName("TextureSet::TextureBulkDataId");

#if WITH_EDITOR
	TEXTURESETSCOMMON_API bool GetSourceDataIdAsString(const UTexture* Texture, FString& StringOut);
#endif
	TEXTURESETSCOMMON_API bool GetSourceDataIdAsString(const FAssetData& AssetData, FString& StringOut);

	TEXTURESETSCOMMON_API TArray<FName> GetUnpackedChannelNames(const TArray<FTextureSetPackedTextureDef>& PackedTextures, const TMap<FName, FTextureSetProcessedTextureDef>& ProcessedTextures);

	TEXTURESETSCOMMON_API IAssetRegistry& GetAssetRegistry();

	/**
	  * Get all assets on disk (in packages) dependent on the given package via the asset registry.
	  * May optionally filter returned values to include assets of a specified UClass (and it's derived classes).
	  * Otherwise, return all found assets.
	  * @param PackageName The package for which to find dependent assets
	  * @param FilterClass Optional. If non-null, returned values will include assets of class FilterClass and it's derived classes
	  * @return	Returns an array of FAssetData, each element corresponding to an asset in a package dependent on the specified package
	  * @see FAssetData
	  */
	TEXTURESETSCOMMON_API TArray<FAssetData> GetDependenciesForPackage(const FName& PackageName, TObjectPtr<const UClass> FilterClass = nullptr);

	/**
	  * Get all on disk (in packages) dependent on the given package via the asset registry, filtered and grouped by the given UClasses.
	  * 
	  * @param PackageName The package for which to find dependent assets.
	  * @param FilterClasses Classes by which to filter and group the return values.
	  * @return	Returns a map of UClasses to arrays of FAssetData. Keys are each of the given UClasses, and values are
	  * the assets of the respective UClass (or derived) type in packages dependent on the given package.
	  * @see FAssetData
	  */
	TEXTURESETSCOMMON_API TMap<TObjectPtr<UClass>, TArray<FAssetData>> GetDependenciesForPackage(const FName& PackageName, TArrayView<const TObjectPtr<UClass>> FilterClasses);	

	TEXTURESETSCOMMON_API FName MakeTextureParameterName(FName ParameterName, int TextureIndex);

	TEXTURESETSCOMMON_API FName MakeConstantParameterName(FName ParameterName, FName ConstantName);

	TEXTURESETSCOMMON_API int32 GetMipCount(FIntVector3 ImageSize, bool bIsVolume);

	TEXTURESETSCOMMON_API FIntVector3 GetMipSize(FIntVector3 Mip0Size, int Mip, bool bIsVolume);

};
