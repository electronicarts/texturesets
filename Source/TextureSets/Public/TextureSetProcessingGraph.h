// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "ProcessingNodes/IProcessingNode.h"
#include "ProcessingNodes/TextureInput.h"

class UTextureSetModule;

class FTextureSetProcessingGraph
{
public:
	FTextureSetProcessingGraph();
	FTextureSetProcessingGraph(const TArray<const UTextureSetModule*>& Modules);

	bool HasGenerated() const { return bHasGenerated; }
	void Regenerate(const TArray<const UTextureSetModule*>& Modules);

	TSharedRef<FTextureInput> AddInputTexture(FName Name, const FTextureSetSourceTextureDef& SourceDef)
	{
		TSharedRef<FTextureInput> NewInput(new FTextureInput(Name, SourceDef));
		InputTextures.Add(Name, NewInput);
		return NewInput;
	}
	const TMap<FName, TSharedRef<FTextureInput>>& GetInputTextures() const { return InputTextures; }

	void AddOutputTexture(FName Name, TSharedRef<ITextureProcessingNode> Texture) { OutputTextures.Add(Name, Texture); }
	const TMap<FName, TSharedRef<ITextureProcessingNode>>& GetOutputTextures() { return OutputTextures; }
	const TMap<FName, const ITextureProcessingNode*> GetOutputTextures() const;

	void AddOutputParameter(FName Name, TSharedRef<IParameterProcessingNode> Parameter) { OutputParameters.Add(Name, Parameter); }
	const TMap<FName, TSharedRef<IParameterProcessingNode>>& GetOutputParameters() { return OutputParameters; }
	const TMap<FName, const IParameterProcessingNode*> GetOutputParameters() const;

	void SetTextureFlagDefault(ETextureSetTextureFlags Flags);
	const ETextureSetTextureFlags GetTextureFlagDefault() const { return TextureFlags; }

private:
	TMap<FName, TSharedRef<FTextureInput>> InputTextures;
	TMap<FName, TSharedRef<ITextureProcessingNode>> OutputTextures;
	TMap<FName, TSharedRef<IParameterProcessingNode>> OutputParameters;

	ETextureSetTextureFlags TextureFlags;

	bool bHasGenerated;
};
#endif
