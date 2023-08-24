// (c) Electronic Arts. All Rights Reserved.

#if WITH_EDITOR

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
#endif
