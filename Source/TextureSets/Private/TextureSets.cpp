// (c) Electronic Arts. All Rights Reserved.

#include "TextureSets.h"

#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FTextureSetsModule"

void FTextureSetsModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("TextureSets"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/TextureSets"), PluginShaderDir);

#if WITH_EDITOR
	FModuleManager::Get().LoadModule("TextureSetsStandardModules");
#endif
}

void FTextureSetsModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTextureSetsModule, TextureSets)
