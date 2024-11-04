// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DerivedDataPluginInterface.h"
#include "TextureSetDerivedData.h"
#include "TextureSetInfo.h"
#include "TextureSetModule.h"
#include "TextureSetProcessingGraph.h"
#include "TextureSetProcessingContext.h"

class UTextureSet;
class FTextureSetCompiler;
class ITextureProcessingNode;

struct FTextureSetCompilerArgs
{
	FTextureSetDefinitionModuleInfo ModuleInfo;
	FTextureSetPackingInfo PackingInfo;
	TMap<FName, FTextureSetSourceTextureReference> SourceTextures;
	FTextureSetAssetParamsCollection AssetParams;
	FString NamePrefix;
	FString DebugContext;
	FString UserKey;
	TObjectPtr<UObject> OuterObject;
};

class TEXTURESETSCOMPILER_API FTextureSetCompiler
{
	friend class TextureSetDerivedTextureDataPlugin;
	friend class TextureSetDerivedParameterDataPlugin;
	friend class FTextureSetCompilerTaskWorker;
public:

	FTextureSetCompiler(TSharedRef<const FTextureSetCompilerArgs> Args);
	~FTextureSetCompiler() {};

	// False if this compiler will produce the same derived data
	bool CompilationRequired(UTextureSetDerivedData* ExistingDerivedData) const;

	// True if this compiler will produce the same derived data as the other compiler
	bool Equivalent(FTextureSetCompiler& OtherCompiler) const;

	void LoadResources();

	void ConfigureTexture(FDerivedTexture& DerivedTexture, int Index) const;
	void InitializeTextureSource(FDerivedTexture& DerivedTexture, int Index) const;
	void GenerateTextureSource(FDerivedTexture& DerivedTexture, int Index) const;
	void FreeTextureSource(FDerivedTexture& DerivedTexture, int Index) const;

	FDerivedParameterData BuildParameterData(FName Name) const;

	FGuid GetTextureDataId(int Index) const;
	FGuid GetParameterDataId(FName Name) const;

	const TSharedRef<const FTextureSetCompilerArgs> Args;

	TArray<FName> GetAllParameterNames() const;

private:
	FTextureSetProcessingContext Context;
	TSharedPtr<FTextureSetProcessingGraph> GraphInstance;

	bool bResourcesLoaded;

	mutable TArray<FGuid> CachedDerivedTextureIds;
	mutable TMap<FName, FGuid> CachedParameterIds;

	FGuid ComputeTextureDataId(int Index) const;
	FGuid ComputeParameterDataId(const TSharedRef<IParameterProcessingNode> Parameter) const;

	static inline int GetPixelIndex(int X, int Y, int Z, int Channel, int Width, int Height, int PixelStride)
	{
		return ((Z * Width * Height) + (Y * Width) + X) * PixelStride + Channel;
	}

};
