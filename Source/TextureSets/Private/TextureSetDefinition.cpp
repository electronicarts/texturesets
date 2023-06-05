// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetDefinition.h"

#include "TextureSet.h"
#include "TextureSetPackedTextureDef.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "ImageUtils.h"

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

const TextureSetTextureDef TextureSetDefinitionSharedInfo::GetProcessedTextureByName(FName Name) const
{
	return ProcessedTextures[ProcessedTextureIndicies.FindChecked(Name)];
}

FVector4 TextureSetPackingInfo::GetDefaultColor(int index) const
{
	return PackedTextureInfos[index].DefaultColor;
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

#if WITH_EDITOR
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
#endif

#if WITH_EDITOR
void UTextureSetDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateDefaultTextures();
}
#endif

void UTextureSetDefinition::PostLoad()
{
	Super::PostLoad();
	UpdateDefaultTextures();
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

const TextureSetPackingInfo UTextureSetDefinition::GetPackingInfo() const
{
	TextureSetPackingInfo PackingInfo;
	TextureSetDefinitionSharedInfo SharedInfo = GetSharedInfo();

	for (int i = 0; i < PackedTextures.Num(); i++)
	{
		const FTextureSetPackedTextureDef& TextureDef = PackedTextures[i];
		TextureSetPackingInfo::TextureSetPackedTextureInfo TextureInfo;

		TArray<FName> SourceNames = TextureDef.GetSources();
		for (int c = 0; c < SourceNames.Num(); c++)
		{
			if (SourceNames[c].IsNone())
				continue;

			FString SourceTexString;
			FString SourceChannelString;
			SourceNames[c].ToString().Split(".", &SourceTexString, &SourceChannelString);
			const FName SourceTexName = FName(SourceTexString);

			static const TMap<FString, int> ChannelStringLookup = {
				{"r", 0},
				{"g", 1},
				{"b", 2},
				{"a", 3},
			};

			const int SourceChannel = ChannelStringLookup.FindChecked(SourceChannelString);

			TextureSetTextureDef Processed = SharedInfo.GetProcessedTextureByName(FName(SourceTexName));

			TextureInfo.ProcessedTextures[c] = SourceTexName;
			TextureInfo.ProessedTextureChannels[c] = SourceChannel;
			TextureInfo.SRGB[c] = Processed.SRGB;
			TextureInfo.DefaultColor[c] = Processed.DefaultValue[SourceChannel];
		}

		PackingInfo.PackedTextureDefs.Add(TextureDef);
		PackingInfo.PackedTextureInfos.Add(TextureInfo);
	}

	return PackingInfo;
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

void UTextureSetDefinition::UpdateDefaultTextures()
{
	DefaultTextures.Empty(PackedTextures.Num());

	TextureSetPackingInfo PackingInfo = GetPackingInfo();

	while (DefaultTextures.Num() < PackedTextures.Num())
	{
		const FTextureSetPackedTextureDef& Def = PackedTextures[DefaultTextures.Num()];

		FString TextureName = "DefaultTexture_" + FString::FromInt(DefaultTextures.Num());
		const int DefaultTextureSize = 4;

		FLinearColor SourceColor = FLinearColor(PackingInfo.GetDefaultColor(DefaultTextures.Num()));

		// TODO linearcolor
		TArray<FColor> DefaultData;

		for (int i = 0; i < (DefaultTextureSize * DefaultTextureSize); i++)
			DefaultData.Add(SourceColor.ToFColor(false));

		EObjectFlags Flags = RF_Public | RF_Transient;

		FCreateTexture2DParameters Params;
		Params.bUseAlpha = Def.UsedChannels() >= 4;
		Params.CompressionSettings = Def.CompressionSettings;
		Params.bDeferCompression = false;
		Params.bSRGB = Def.GetHardwareSRGBEnabled();
		Params.bVirtualTexture = false;
		//Params.MipGenSettings;
		//Params.TextureGroup;

		UTexture2D* DefaultTexture = FImageUtils::CreateTexture2D(4, 4, DefaultData, this, TextureName, Flags, Params);

		DefaultTextures.Add(DefaultTexture);
	}
}

UTexture* UTextureSetDefinition::GetDefaultPackedTexture(int index) const
{
	return DefaultTextures[index];
}

int32 UTextureSetDefinition::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression)
{
	uint32 Hash = 0;

	TArray<TSubclassOf<UTextureSetSampleParams>> RequiredTypes;
	for (UTextureSetDefinitionModule* Module : Modules)
	{
		Hash = HashCombine(Hash, Module->ComputeSamplingHash(SampleExpression));
	}

	return Hash;
}

#if WITH_EDITOR
void UTextureSetDefinition::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
	FTextureSetMaterialGraphBuilder& Builder) const
{
	TArray<TSubclassOf<UTextureSetSampleParams>> RequiredTypes;
	for (UTextureSetDefinitionModule* Module : Modules)
	{
		Module->GenerateSamplingGraph(SampleExpression, Builder);
	}
}
#endif
