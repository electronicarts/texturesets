// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "TextureSetElementCollectionFactory.generated.h"

UCLASS()
class UTextureSetElementCollectionFactory : public UFactory
{
	GENERATED_BODY()
	
	UTextureSetElementCollectionFactory(const FObjectInitializer& ObjectInitializer);
	
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

	virtual bool ConfigureProperties() override {return true;}
};
