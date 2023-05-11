// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TextureSetDefinition.generated.h"

/**
 * These elements make up the list represented by Texture Set Data Assets
 */

USTRUCT()
struct FTextureDefinitionInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (GetOptions = "GetTypeOptions", NoElementDuplicate))
	FString TextureTypes;

	UPROPERTY(EditAnywhere, Category=Components)
	bool R = true;
	UPROPERTY(EditAnywhere, Category=Components)
	bool G = false;
	UPROPERTY(EditAnywhere, Category=Components)
	bool B = false;
	UPROPERTY(EditAnywhere, Category=Components)
	bool A = false;
	/*
	UPROPERTY(EditAnywhere)
	FString itemName;
 
	UPROPERTY(EditAnywhere)
	UTexture2D* itemThumbnail;
 
	UPROPERTY(EditAnywhere)
	UBlueprint* itemBlueprint;
 
	UPROPERTY(EditAnywhere)
	FColor itemColor;*/
};

UCLASS()
class TEXTURESETS_API UTextureSetDefinition : public UDataAsset
{
	GENERATED_BODY()
	
	UFUNCTION(CallInEditor)
	TArray<FString> GetTypeOptions() const
	{
		return { TEXT("BaseColor"), TEXT("Metalness"), TEXT("Smoothness") };
	}

	void GenerateMaterialFunction();

public:
	UPROPERTY(EditAnywhere)
	TArray<FTextureDefinitionInfo> Items;

	UPROPERTY()
	TObjectPtr<UMaterialFunction> SamplingMaterialFunction;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
};

UCLASS()
class UTextureSetDefinitionFactory : public UFactory
{
	GENERATED_BODY()
	UTextureSetDefinitionFactory(const FObjectInitializer& ObjectInitializer);

public:
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
