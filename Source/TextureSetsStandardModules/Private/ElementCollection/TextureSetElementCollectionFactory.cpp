// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetElementCollectionFactory.h"
#include "ElementCollection/TextureSetElementCollection.h"

#include "TextureSet.h"

UTextureSetElementCollectionFactory::UTextureSetElementCollectionFactory(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SupportedClass = UTextureSetElementCollectionAsset::StaticClass();

	bCreateNew = true;
	bText = false;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UTextureSetElementCollectionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UTextureSetElementCollectionAsset>(InParent, Class, Name, Flags);
}