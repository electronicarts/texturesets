// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetInfo.h"

#include "TextureSetsHelpers.h"
#include "TextureSetModule.h"
#include "ProcessingNodes/TextureInput.h"

#define LOCTEXT_NAMESPACE "TextureSets"

#if WITH_EDITOR
FTextureSetDefinitionModuleInfo::FTextureSetDefinitionModuleInfo(const TArray<const UTextureSetModule*>& ModulesIn)
	: Modules(ModulesIn)
{
	ProcessingGraph = MakeShared<FTextureSetProcessingGraph>(Modules);

	for (const auto&[Name, Input] : ProcessingGraph->GetInputTextures())
	{
		SourceTextures.Add(Name, Input->GetTextureDef());
	}

	for (const auto&[Name, Output] : ProcessingGraph->GetOutputTextures())
	{
		ProcessedTextures.Add(Name, Output->GetTextureDef());
	}
}
#endif

FTextureSetDefinitionModuleInfo::FTextureSetDefinitionModuleInfo()
{
	// Create a blank processing graph, it will be regenerated if GetProcessingGraph() is called
	ProcessingGraph = MakeShared<FTextureSetProcessingGraph>();
}

#if WITH_EDITOR
const FTextureSetProcessingGraph* FTextureSetDefinitionModuleInfo::GetProcessingGraph() const
{
	check(ProcessingGraph.IsValid());

	// Processing graph is not serialized so it may need to be regenerated
	if (!ProcessingGraph->HasGenerated())
		ProcessingGraph->Regenerate(Modules);

	return ProcessingGraph.Get();
}
#endif

#if WITH_EDITOR
FTextureSetPackingInfo::FTextureSetPackingInfo(const TArray<FTextureSetPackedTextureDef>& PackedTextures, const FTextureSetDefinitionModuleInfo& ModuleInfo)
{
	TArray<FName> PackedChannels;

	for (int i = 0; i < PackedTextures.Num(); i++)
	{
		const FTextureSetPackedTextureDef& TextureDef = PackedTextures[i];

		if (TextureDef.GetUsedChannels() > TextureDef.GetAvailableChannels())
			Errors.Add(FText::Format(LOCTEXT("OverpackedTexture","Packed texture {0} is specifying more packed channels than are provided by the chosen compression format."), i));

		FTextureSetPackedTextureInfo TextureInfo;

		// Enabled initially, but can be disabled if it's found to be incompatible with the current packing
		TextureInfo.HardwareSRGB = TextureDef.GetHardwareSRGBSupported();

		TextureInfo.ChannelEncodings = (uint8)ETextureSetChannelEncoding::None;

		bool bIsFirstSource = true;

		for (int c = 0; c < TextureDef.GetSources().Num(); c++)
		{
			FName Source = TextureDef.GetSources()[c];

			if (Source.IsNone())
				continue;

			if (PackedChannels.Contains(Source))
			{
				Errors.Add(FText::Format(LOCTEXT("PackingDuplication","Processed texture {0} appears more than once in the packing definition."), FText::FromName(Source)));
				continue;
			}

			PackedChannels.AddUnique(Source);

			FString SourceTexString;
			FString SourceChannelString;
			Source.ToString().Split(".", &SourceTexString, &SourceChannelString);
			const FName SourceTexName = FName(SourceTexString);

			if (!ModuleInfo.GetProcessedTextures().Contains(SourceTexName))
			{
				// Trying to pack a channel from a texture that doesn't exist
				Errors.Add(FText::Format(LOCTEXT("NonExistingSource", "Processed texture '{0}' does not exist and cannot be packed."), {FText::FromName(SourceTexName)}));
				continue;
			}

			const int SourceChannel = TextureSetsHelpers::ChannelSuffixes.Find("." + SourceChannelString);
			check (SourceChannel != INDEX_NONE);

			FTextureSetProcessedTextureDef Processed = ModuleInfo.GetProcessedTextures().FindChecked(SourceTexName);

			if (SourceChannel >= Processed.ChannelCount)
			{
				// Trying to pack a channel that doesn't exist in the processed texture output
				Warnings.Add(FText::Format(LOCTEXT("NonExistingSourceChannel", "Processed texture '{0}' only has {1} channels, but '{2}' exists in packed texture {3}."), 
					{FText::FromName(SourceTexName), Processed.ChannelCount, FText::FromName(Source), i}));
				continue;
			}

			TextureInfo.ChannelInfo[c].ProcessedTexture = SourceTexName;
			TextureInfo.ChannelInfo[c].ProessedTextureChannel = SourceChannel;
			TextureInfo.ChannelInfo[c].ChannelEncoding = Processed.Encoding;
			TextureInfo.ChannelEncodings |= Processed.Encoding;

			if (c < 3 && !(Processed.Encoding & (uint8)ETextureSetChannelEncoding::SRGB))
				TextureInfo.HardwareSRGB = false; // If we have any non-sRGB textures in R, G, or B, then we can't use hardware SRGB.

			if (bIsFirstSource)
			{
				TextureInfo.Flags = Processed.Flags;
				bIsFirstSource = false;
			}
			else if (TextureInfo.Flags != Processed.Flags)
			{
				Errors.Add(FText::Format(LOCTEXT("MismatchedFlags", "Not all sources in packed texture {0} share the same texture type."), {i}));
			}

			// Record packing sources for lookup later.
			PackingSource.Add(Source, TTuple<int, int>(i, c));
		}

		// Set channel count after loop, incase any channels were removed
		TextureInfo.ChannelCount = TextureDef.GetSources().Num();
		TextureInfo.RangeCompressMulName = FName("RangeCompress_" + FString::FromInt(i) + "_Mul");
		TextureInfo.RangeCompressAddName = FName("RangeCompress_" + FString::FromInt(i) + "_Add");

		PackedTextureDefs.Add(TextureDef);
		PackedTextureInfos.Add(TextureInfo);
	}

	for (FName Name : TextureSetsHelpers::GetUnpackedChannelNames(PackedTextureDefs, ModuleInfo.GetProcessedTextures()))
	{
		if (Name.IsNone())
			continue;

		Errors.Add(FText::Format(LOCTEXT("UnpackedChannel","\"{0}\" is unused, did you forget to pack it?"), FText::FromName(Name)));
	}
}
#endif

#undef LOCTEXT_NAMESPACE