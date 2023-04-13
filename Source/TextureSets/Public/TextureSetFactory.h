// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorFactories/ActorFactory.h"
#include "TextureSetFactory.generated.h"

/**
 * 
 */
UCLASS()
class UTextureSetFactory : public UFactory
{
	GENERATED_BODY()
	
	UTextureSetFactory(const FObjectInitializer& ObjectInitializer);
	
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateBinary(
		UClass * InClass,
		UObject * InParent,
		FName InName,
		EObjectFlags Flags,
		UObject * Context,
		const TCHAR * Type,
		const uint8 *& Buffer,
		const uint8 * BufferEnd,
		FFeedbackContext * Warn) override;

	virtual UObject * ImportObject(
		UClass * InClass,
		UObject * InOuter,
		FName InName,
		EObjectFlags Flags,
		const FString & Filename,
		const TCHAR * Parms,
		bool & OutCanceled) override;

	virtual bool ConfigureProperties() override {return true;}
};
