// (c) Electronic Arts. All Rights Reserved.

#pragma once

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

	void AddOutputTexture(FName Name, TSharedRef<IProcessingNode> Texture) { OutputTextures.Add(Name, Texture); }
	const TMap<FName, TSharedRef<IProcessingNode>>& GetOutputTextures() const { return OutputTextures; }

private:
	TMap<FName, TSharedRef<FTextureInput>> InputTextures;
	TMap<FName, TSharedRef<IProcessingNode>> OutputTextures;
};
