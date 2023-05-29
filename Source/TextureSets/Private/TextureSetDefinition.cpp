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

void TextureSetDefinitionSharedInfo::AddSourceTexture(const TextureSetTextureDef& Texture)
{
	SourceTextures.Add(Texture);
}

void TextureSetDefinitionSharedInfo::AddProcessedTexture(const TextureSetTextureDef& Texture)
{
	checkf(!ProcessedTextureIndicies.Contains(Texture.Name), TEXT("Attempting to add processed texture %s twice"), Texture.Name);
	ProcessedTextureIndicies.Add(Texture.Name, ProcessedTextures.Num());
	ProcessedTextures.Add(Texture);
}

const TArray<TextureSetTextureDef> TextureSetDefinitionSharedInfo::GetSourceTextures() const
{
	return SourceTextures;
}

const TArray<TextureSetTextureDef> TextureSetDefinitionSharedInfo::GetProcessedTextures() const
{
	return ProcessedTextures;
}

int TextureSetDefinitionSharedInfo::GetProcessedTextureIndex(FName Name)
{
	return ProcessedTextureIndicies.FindChecked(Name);
}

void TextureSetDefinitionSamplingInfo::AddMaterialParameter(FName Name, EMaterialValueType Type)
{
	checkf(!MaterialParameters.Contains(Name), TEXT("Attempting to add shader constant %s twice"), Name);
	MaterialParameters.Add(Name, Type);
}

void TextureSetDefinitionSamplingInfo::AddSampleInput(FName Name, EMaterialValueType Type)
{
	checkf(!SampleInputs.Contains(Name), TEXT("Attempting to add sample arguemnt %s twice"), Name);
	SampleInputs.Add(Name, Type);
}

void TextureSetDefinitionSamplingInfo::AddSampleOutput(FName Name, EMaterialValueType Type)
{
	checkf(!SampleOutputs.Contains(Name), TEXT("Attempting to add sample result %s twice"), Name);
	SampleOutputs.Add(Name, Type);
}

const TMap<FName, EMaterialValueType> TextureSetDefinitionSamplingInfo::GetMaterialParameters() const
{
	return MaterialParameters;
}

const TMap<FName, EMaterialValueType> TextureSetDefinitionSamplingInfo::GetSampleInputs() const
{
	return SampleInputs;
}

const TMap<FName, EMaterialValueType> TextureSetDefinitionSamplingInfo::GetSampleOutputs() const
{
	return SampleOutputs;
}


const FString UTextureSetDefinition::ChannelSuffixes[4] = {".r", ".g", ".b", ".a"};

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

	for (const TextureSetTextureDef& Tex : GetSharedInfo().GetProcessedTextures())
	{
		for (int i = 0; i < Tex.ChannelCount; i++)
		{
			FName ChannelName = FName(Tex.Name.ToString() + ChannelSuffixes[i]);

			if (!PackedNames.Contains(ChannelName))
				UnpackedNames.Add(ChannelName);
		}
	}

	return UnpackedNames;
}

const TextureSetDefinitionSharedInfo UTextureSetDefinition::GetSharedInfo() const
{
	TextureSetDefinitionSharedInfo Info;

	for (UTextureSetDefinitionModule* Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->BuildSharedInfo(Info);
		}
	}

	return Info;
}

const TextureSetDefinitionSamplingInfo UTextureSetDefinition::GetSamplingInfo(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	TextureSetDefinitionSamplingInfo Info;

	for (UTextureSetDefinitionModule* Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->BuildSamplingInfo(Info, SampleExpression);
		}
	}

	return Info;
}

TArray<TSubclassOf<UTextureSetAssetParams>> UTextureSetDefinition::GetRequiredAssetParamClasses() const
{
	TArray<TSubclassOf<UTextureSetAssetParams>> RequiredTypes;
	for (UTextureSetDefinitionModule* Module : Modules)
	{
		if (IsValid(Module))
		{
			UClass* SampleParamClass = Module->GetAssetParamClass();

			if (SampleParamClass != nullptr)
				if (!RequiredTypes.Contains(SampleParamClass))
					RequiredTypes.Add(SampleParamClass);
		}
	}
	return RequiredTypes;
}

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

UTexture* UTextureSetDefinition::GetDefaultPackedTexture(int index) const
{
	// TODO: Actually get a small texture with the correct default colors.
	return LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"), nullptr, LOAD_None, nullptr);
}

void UTextureSetDefinition::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	TArray<TSubclassOf<UTextureSetSampleParams>> RequiredTypes;
	for (UTextureSetDefinitionModule* Module : Modules)
	{
		Module->GenerateSamplingGraph(SampleExpression, Builder);
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