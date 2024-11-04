// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetsBaker.h"

#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FTextureSetsBakerModule"

void FTextureSetsBakerModule::StartupModule()
{
	
}

void FTextureSetsBakerModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTextureSetsBakerModule, TextureSetsBaker)
