// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetFactory.h"

#include "TextureSet.h"

UTextureSetFactory::UTextureSetFactory(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SupportedClass = UTextureSet::StaticClass();
	Formats.Add(TEXT("psd;My custom asset extension"));

	bCreateNew = true;
	bText = false;
	bEditorImport = true;
	bEditAfterNew = true;
	ImportPriority = INT_MAX;
}

bool UTextureSetFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);

	if( Extension == TEXT("psd"))
	{
		return true;
	}
	return false;
}

UObject* UTextureSetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UTextureSet>(InParent, Class, Name, Flags);
}

UObject* UTextureSetFactory::FactoryCreateBinary(
		UClass * InClass,
		UObject * InParent,
		FName InName,
		EObjectFlags Flags,
		UObject * Context,
		const TCHAR * Type,
		const uint8 *& Buffer,
		const uint8 * BufferEnd,
		FFeedbackContext * Warn)
{
	UTextureSet* TextureSet = NewObject<UTextureSet>(InParent, InClass, InName, Flags);

	//Do whatever initialization you need to do here
	return TextureSet;
}

UObject* UTextureSetFactory::ImportObject(UClass* InClass, UObject* InOuter, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, bool& OutCanceled)
{
	//return Super::ImportObject(InClass, InOuter, InName, Flags, Filename, Parms, OutCanceled);
	UTextureSet* TextureSet = NewObject<UTextureSet>(InOuter, InClass, InName, Flags);

	//Do whatever initialization you need to do here
	return TextureSet;
}
