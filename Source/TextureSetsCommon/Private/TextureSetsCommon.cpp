// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetsCommon.h"

#include "Interfaces/IPluginManager.h"

void FTextureSetsCommonModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("TextureSets"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/TextureSets"), PluginShaderDir);
}

void FTextureSetsCommonModule::ShutdownModule()
{

}

IMPLEMENT_MODULE(FTextureSetsCommonModule, TextureSetsCommon)
