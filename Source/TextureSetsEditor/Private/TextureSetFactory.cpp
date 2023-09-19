// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetFactory.h"

#include "TextureSet.h"

UTextureSetFactory::UTextureSetFactory(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SupportedClass = UTextureSet::StaticClass();

	bCreateNew = true;
	bText = false;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UTextureSetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UTextureSet>(InParent, Class, Name, Flags);
}