// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "TextureSetFactory.generated.h"

UCLASS()
class UTextureSetFactory : public UFactory
{
	GENERATED_BODY()
	
	UTextureSetFactory(const FObjectInitializer& ObjectInitializer);
	
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

	virtual bool ConfigureProperties() override {return true;}
};
