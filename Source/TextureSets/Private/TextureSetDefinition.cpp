// (c) Electronic Arts. All Rights Reserved.


#include "TextureSetDefinition.h"

#include "EditorAssetLibrary.h"
#include "TextureSet.h"
#include "TextureSetEditingUtils.h"
#include <MaterialEditorUtilities.h>
#include <MaterialEditingLibrary.h>
#include <Materials/MaterialExpressionFunctionInput.h>
#include <Materials/MaterialExpressionFunctionOutput.h>
#include <Materials/MaterialExpressionTextureSampleParameter2D.h>
#include <MaterialPropertyHelpers.h>

void UTextureSetDefinition::GenerateMaterialFunction()
{
	TObjectPtr<UMaterialFunction> NewFunction = NewObject<UMaterialFunction>();

	TObjectPtr<UMaterialExpressionFunctionInput> UVExpression = Cast<UMaterialExpressionFunctionInput>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(NewFunction, UMaterialExpressionFunctionInput::StaticClass()));
	UVExpression->InputType = EFunctionInputType::FunctionInput_Vector2;
	UVExpression->InputName = TEXT("UV");

	for (const auto& TextureInfo : Items)
	{

		// Output for the texture
		TObjectPtr<UMaterialExpressionFunctionOutput> OutputExpression = Cast<UMaterialExpressionFunctionOutput>(UMaterialEditingLibrary::CreateMaterialExpressionInFunction(NewFunction, UMaterialExpressionFunctionOutput::StaticClass()));
		OutputExpression->OutputName = FName(TextureInfo.TextureTypes);
		NewFunction->GetExpressionCollection().AddExpression(OutputExpression);

		TObjectPtr<UMaterialExpression> SampleExpression = UMaterialEditingLibrary::CreateMaterialExpressionInFunction(NewFunction, UMaterialExpressionTextureSampleParameter2D::StaticClass());
		SampleExpression->SetParameterName(FName(TextureInfo.TextureTypes));
		FMaterialParameterMetadata meta;
		meta.Value.Type = EMaterialParameterType::Texture;
		meta.Value.Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"), nullptr, LOAD_None, nullptr);
		meta.Group = FMaterialPropertyHelpers::TextureSetParamName;
		meta.SortPriority = 0;
		SampleExpression->SetParameterValue(FName(TextureInfo.TextureTypes), meta, EMaterialExpressionSetParameterValueFlags::AssignGroupAndSortPriority);
		SampleExpression->UpdateParameterGuid(true, true);
		// Material->AddExpressionParameter(RefExpression, Material->EditorParameters);
		// Material->GetExpressionCollection().AddExpression(RefExpression);
		SampleExpression->ConnectExpression(OutputExpression->GetInput(0), 0);

		UVExpression->ConnectExpression(SampleExpression->GetInput(0), 0);
	}

#if WITH_EDITOR
	UMaterialEditingLibrary::LayoutMaterialFunctionExpressions(NewFunction);
#endif
	SamplingMaterialFunction = NewFunction;
}

void UTextureSetDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// TArray<FName> HardDependencies = FTextureSetEditingUtils::FindReferencers(GetPackage()->GetFName());
	//
	// for (auto Dependency : HardDependencies)
	// {
	// 	UObject* Referencer = UEditorAssetLibrary::LoadAsset(Dependency.ToString());
	// 	if (!Referencer->IsA(UTextureSet::StaticClass()))
	// 	{
	// 		continue;
	// 	}
	//
	// 	TObjectPtr<UTextureSet> TextureSet = Cast<UTextureSet>(Referencer);
	// 	TextureSet->Modify();
	// 	TextureSet->UpdateFromDefinition();
	//
	// 	UEditorAssetLibrary::SaveLoadedAsset(Referencer);
	// }
}

void UTextureSetDefinition::PostLoad()
{
	GenerateMaterialFunction();
	Super::PostLoad();
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
