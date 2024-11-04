// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetDefinitionFactory.h"

#include "TextureSetDefinition.h"

UTextureSetDefinitionFactory::UTextureSetDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UTextureSetDefinition::StaticClass();

	bCreateNew = true;
	bText = false;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UTextureSetDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UTextureSetDefinition>(InParent, Class, Name, Flags);
}
