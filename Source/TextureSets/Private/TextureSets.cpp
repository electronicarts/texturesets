// (c) Electronic Arts. All Rights Reserved.

#include "TextureSets.h"

#include "TextureSetsMaterialInstanceUserData.h"

#define LOCTEXT_NAMESPACE "FTextureSetsModule"

TAutoConsoleVariable<bool> CVarVisualizeMaterialGraph(
	TEXT("TextureSet.VisualizeInMaterialGraph"),
	false,
	TEXT("Draw debug widgets in material graphs. Requires graphs to be re-opened\n"),
	ECVF_Default);

void FTextureSetsModule::StartupModule()
{
	UTextureSetsMaterialInstanceUserData::RegisterCallbacks();
}

void FTextureSetsModule::ShutdownModule()
{
	UTextureSetsMaterialInstanceUserData::UnregisterCallbacks();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTextureSetsModule, TextureSets)
