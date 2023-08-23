// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetProcessingGraph.h"
#include "TextureSetModule.h"
#include "TextureSetProcessingContext.h"

FTextureSetProcessingGraph::FTextureSetProcessingGraph(TArray<const UTextureSetModule*> Modules)
{
	for (const UTextureSetModule* Module : Modules)
	{
		Module->GenerateProcessingGraph(*this);
	}
}