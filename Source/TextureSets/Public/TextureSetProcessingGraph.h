// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "ProcessingNodes/IProcessingNode.h"
#include "ProcessingNodes/TextureInput.h"

class FTextureSetProcessingGraph
{
public:
	FTextureSetProcessingGraph(TArray<const class UTextureSetModule*> Modules);

	TSharedRef<FTextureInput> AddInputTexture(FName Name, const FTextureSetSourceTextureDef& SourceDef)
	{
		TSharedRef<FTextureInput> NewInput(new FTextureInput(Name, SourceDef));
		InputTextures.Add(Name, NewInput);
		return NewInput;
	}
	const TMap<FName, TSharedRef<FTextureInput>>& GetInputTextures() const { return InputTextures; }

	void AddOutputTexture(FName Name, TSharedRef<ITextureProcessingNode> Texture) { OutputTextures.Add(Name, Texture); }
	const TMap<FName, TSharedRef<ITextureProcessingNode>>& GetOutputTextures() const { return OutputTextures; }

	void AddOutputParameter(FName Name, TSharedRef<IParameterProcessingNode> Parameter) { OutputParameters.Add(Name, Parameter); }
	const TMap<FName, TSharedRef<IParameterProcessingNode>>& GetOutputParameters() const { return OutputParameters; }
private:
	TMap<FName, TSharedRef<FTextureInput>> InputTextures;
	TMap<FName, TSharedRef<ITextureProcessingNode>> OutputTextures;
	TMap<FName, TSharedRef<IParameterProcessingNode>> OutputParameters;
};
#endif
