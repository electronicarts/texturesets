// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "TextureSetDefinitionFactory.generated.h"

UCLASS()
class UTextureSetDefinitionFactory : public UFactory
{
	GENERATED_BODY()
		UTextureSetDefinitionFactory(const FObjectInitializer& ObjectInitializer);

public:
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};