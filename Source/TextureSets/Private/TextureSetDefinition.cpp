// (c) Electronic Arts. All Rights Reserved.


#include "TextureSetDefinition.h"

#include "EditorAssetLibrary.h"
#include "TextureSet.h"
#include "TextureSetEditingUtils.h"

void UTextureSetDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	TArray<FName> HardDependencies = FTextureSetEditingUtils::FindReferencers(GetPackage()->GetFName());

	for (auto Dependency : HardDependencies)
	{
		UObject* Referencer = UEditorAssetLibrary::LoadAsset(Dependency.ToString());
		if (!Referencer->IsA(UTextureSet::StaticClass()))
		{
			continue;
		}

		TObjectPtr<UTextureSet> TextureSet = Cast<UTextureSet>(Referencer);
		TextureSet->Modify();
		TextureSet->UpdateFromDefinition();

		UEditorAssetLibrary::SaveLoadedAsset(Referencer);
	}
}

UTextureSetDefinitionFactory::UTextureSetDefinitionFactory(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SupportedClass = UTextureSetDefinition::StaticClass();

	bCreateNew    = true;
	bText         = false;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UTextureSetDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UTextureSetDefinition>(InParent, Class, Name, Flags);
}
