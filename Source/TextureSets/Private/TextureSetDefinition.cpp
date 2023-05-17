// (c) Electronic Arts. All Rights Reserved.


#include "TextureSetDefinition.h"

#include "EditorAssetLibrary.h"
#include "TextureSet.h"
#include "TextureSetPackedTextureDef.h"
#include "TextureSetEditingUtils.h"
#include <MaterialEditorUtilities.h>
#include <MaterialEditingLibrary.h>
#include <Materials/MaterialExpressionFunctionInput.h>
#include <Materials/MaterialExpressionFunctionOutput.h>
#include <Materials/MaterialExpressionTextureSampleParameter2D.h>
#include <MaterialPropertyHelpers.h>
#include "MaterialExpressionTextureSetSampleParameter.h"

TObjectPtr<UMaterialFunction> UTextureSetDefinition::GenerateMaterialFunction(UMaterialExpressionTextureSetSampleParameter* ExpressionInstance)
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
	return NewFunction;
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
	//GenerateMaterialFunction();
	Super::PostLoad();
}

bool UTextureSetDefinition::CanEditChange(const FProperty* InProperty) const
{
	// If other logic prevents editing, we want to respect that
	const bool SuperValue = Super::CanEditChange(InProperty);

	if (InProperty->GetOwnerStruct() == FTextureSetPackedTextureDef::StaticStruct())
	{
		const FTextureSetPackedTextureDef* TextureDef = nullptr; // TODO: Have to get this from the property somehow

		if (TextureDef)
		{
			if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FTextureSetPackedTextureDef, SourceR))
			{
				return SuperValue && TextureDef->AvailableChannels() > 0;
			}

			if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FTextureSetPackedTextureDef, SourceG))
			{
				return SuperValue && TextureDef->AvailableChannels() > 1;
			}

			if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FTextureSetPackedTextureDef, SourceB))
			{
				return SuperValue && TextureDef->AvailableChannels() > 2;
			}

			if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FTextureSetPackedTextureDef, SourceA))
			{
				return SuperValue && TextureDef->AvailableChannels() > 3;
			}
		}
	}

	return SuperValue;
}

TArray<FName> UTextureSetDefinition::GetUnpackedChannelNames() const
{
	// Construct an array of all the channel names already used for packing
	TArray<FName> PackedNames = TArray<FName>();

	for (const FTextureSetPackedTextureDef& Tex : PackedTextures)
	{
		if (!Tex.SourceR.IsNone())
			PackedNames.Add(Tex.SourceR);

		if (!Tex.SourceG.IsNone())
			PackedNames.Add(Tex.SourceG);

		if (!Tex.SourceB.IsNone())
			PackedNames.Add(Tex.SourceB);

		if (!Tex.SourceA.IsNone())
			PackedNames.Add(Tex.SourceA);
	}

	// Construct a list of channel names not yet used for packing (remaining choices)
	TArray<FName> UnpackedNames = TArray<FName>{  FName() };

	for (const TextureSetTextureDef& Tex : GetProcesedTextures())
	{
		for (int i = 0; i < Tex.ChannelCount; i++)
		{
			FString ChannelSuffixes[4] = {".r", ".g", ".b", ".a"};
			FName ChannelName = FName(Tex.Name.ToString() + (Tex.ChannelCount > 1 ? ChannelSuffixes[i] : ""));

			if (!PackedNames.Contains(ChannelName))
				UnpackedNames.Add(ChannelName);
		}
	}

	return UnpackedNames;
}

TArray<TextureSetParameterDef> UTextureSetDefinition::GetSourceParameters() const
{
	TArray<TextureSetParameterDef> SourceParams;
	for (UTextureSetDefinitionModule* Module : Modules)
	{
		if (IsValid(Module))
			SourceParams.Append(Module->GetSourceParameters());
	}
	return SourceParams;
};

TArray<TextureSetTextureDef> UTextureSetDefinition::GetSourceTextures() const
{
	TArray<TextureSetTextureDef> SourceTextures;
	for (UTextureSetDefinitionModule* Module : Modules)
	{
		if (IsValid(Module))
			SourceTextures.Append(Module->GetSourceTextures());
	}
	return SourceTextures;
};

TArray<TextureSetTextureDef> UTextureSetDefinition::GetProcesedTextures() const
{
	TArray<TextureSetTextureDef> SourceTextures;
	for (UTextureSetDefinitionModule* Module : Modules)
	{
		if (IsValid(Module))
			SourceTextures.Append(Module->GetProcessedTextures());
	}
	return SourceTextures;
};

TMap<FString, EMaterialValueType> UTextureSetDefinition::GetShaderConstants() const
{
	TMap<FString, EMaterialValueType> ShaderConstants;
	for (UTextureSetDefinitionModule* Module : Modules)
	{
		if (IsValid(Module))
			ShaderConstants.Append(Module->GetShaderConstants());
	}
	return ShaderConstants;
};

TArray<OutputElementDef> UTextureSetDefinition::GetOutputElements(TArray<UTextureSetSampleParams*> SampleParamsArray) const
{
	TArray<OutputElementDef> OutputElements;
	for (UTextureSetDefinitionModule* Module : Modules)
	{
		if (IsValid(Module))
		{
			UTextureSetSampleParams* SampleParams = nullptr;

			for (UTextureSetSampleParams* Params : SampleParamsArray)
			{
				if (Params->GetClass() == Module->GetSampleParamClass())
				{
					SampleParams = Params;
					break;
				}
			}

			OutputElements.Append(Module->GetOutputElements(SampleParams));
		}
	}
	return OutputElements;
};

TArray<TSubclassOf<UTextureSetSampleParams>> UTextureSetDefinition::GetRequiredSampleParamClasses() const
{
	TArray<TSubclassOf<UTextureSetSampleParams>> RequiredTypes;
	for (UTextureSetDefinitionModule* Module : Modules)
	{
		if (IsValid(Module))
		{
			UClass* SampleParamClass = Module->GetSampleParamClass();

			if (SampleParamClass != nullptr)
				if (!RequiredTypes.Contains(SampleParamClass))
					RequiredTypes.Add(SampleParamClass);
		}
	}
	return RequiredTypes;
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
