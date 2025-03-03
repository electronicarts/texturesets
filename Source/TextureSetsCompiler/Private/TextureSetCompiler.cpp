// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetCompiler.h"

#include "Async/ParallelFor.h"
#include "DerivedDataBuildVersion.h"
#include "DerivedDataCacheInterface.h"
#include "ProcessingNodes/TextureOperatorEnlarge.h"
#include "TextureSetDerivedData.h"
#include "TextureSetsHelpers.h"

#define BENCHMARK_TEXTURESET_COMPILATION 1

FTextureSetCompiler::FTextureSetCompiler(TSharedRef<const FTextureSetCompilerArgs> Args)
	: Args(Args)
	, bResourcesLoaded(false)
{
	check(IsInGameThread());
	
	Context.SourceTextures = Args->SourceTextures;
	Context.AssetParams = Args->AssetParams;

	// TODO: Get module list out of module info
	GraphInstance = MakeShared<FTextureSetProcessingGraph>(Args->ModuleInfo.GetModules());

	CachedDerivedTextureIds.SetNum(Args->PackingInfo.NumPackedTextures());
}

bool FTextureSetCompiler::CompilationRequired(UTextureSetDerivedData* ExistingDerivedData) const
{
	if (ExistingDerivedData == nullptr)
		return true;

	if (ExistingDerivedData->Textures.Num() != Args->PackingInfo.NumPackedTextures())
		return true; // Needs to add or remove a derived texture

	for (int t = 0; t < Args->PackingInfo.NumPackedTextures(); t++)
	{
		if (GetTextureDataId(t) != ExistingDerivedData->Textures[t].Data.Id)
			return true; // Some texture data needs to be updated
	}

	for (FName Name : GetAllParameterNames())
	{
		const FDerivedParameterData* ParameterData = ExistingDerivedData->MaterialParameters.Find(Name);

		if (!ParameterData || (GetParameterDataId(Name) != ParameterData->Id))
			return true; // Some parameter data needs to be updated
	}

	return false;
}

bool FTextureSetCompiler::Equivalent(FTextureSetCompiler& OtherCompiler) const
{
	if (OtherCompiler.Args->PackingInfo.NumPackedTextures() != Args->PackingInfo.NumPackedTextures())
		return false; // Needs to add or remove a derived texture

	for (int t = 0; t < Args->PackingInfo.NumPackedTextures(); t++)
	{
		if (GetTextureDataId(t) != OtherCompiler.GetTextureDataId(t))
			return false; // Some texture data needs to be updated
	}

	for (FName Name : OtherCompiler.GetAllParameterNames())
	{
		if (!GraphInstance->GetOutputParameters().Contains(Name))
			return false; // Have different parameters

		if (GetParameterDataId(Name) != OtherCompiler.GetParameterDataId(Name))
			return false; // Some parameter data needs to be updated
	}

	return true;
}

FGuid FTextureSetCompiler::GetTextureDataId(int Index) const
{
	if (!CachedDerivedTextureIds[Index].IsValid())
		CachedDerivedTextureIds[Index] = ComputeTextureDataId(Index);

	return CachedDerivedTextureIds[Index];
}

FGuid FTextureSetCompiler::GetParameterDataId(FName Name) const
{
	if (!CachedParameterIds.Contains(Name))
	{
		const TSharedRef<IParameterProcessingNode>& Parameter = GraphInstance->GetOutputParameters().FindChecked(Name);
		CachedParameterIds.Add(Name, ComputeParameterDataId(Parameter));
	}

	return CachedParameterIds.FindChecked(Name);
}

TArray<FName> FTextureSetCompiler::GetAllParameterNames() const
{
	TArray<FName> AllNames;
	AllNames.Reserve(GraphInstance->GetOutputParameters().Num());

	for (const auto& [Name, Node] : GraphInstance->GetOutputParameters())
		AllNames.Add(Name);

	return AllNames;
}

FGuid FTextureSetCompiler::ComputeTextureDataId(int PackedTextureIndex) const
{
	check(PackedTextureIndex < Args->PackingInfo.NumPackedTextures());

	UE::DerivedData::FBuildVersionBuilder IdBuilder;

	IdBuilder << FString("TextureSetDerivedTexture_V0.21"); // Version string, bump this to invalidate everything
	IdBuilder << Args->UserKey; // Key for debugging, easily force rebuild
	IdBuilder << GetTypeHash(Args->PackingInfo.GetPackedTextureDef(PackedTextureIndex));

	TSet<FName> TextureDependencies;
	for (const FTextureSetPackedChannelInfo& ChannelInfo : Args->PackingInfo.GetPackedTextureInfo(PackedTextureIndex).ChannelInfo)
	{
		if (!ChannelInfo.ProcessedTexture.IsNone())
			TextureDependencies.Add(ChannelInfo.ProcessedTexture);
	}

	// Only hash on source textures that contribute to this packed texture
	for (const FName& TextureName : TextureDependencies)
	{
		const TSharedRef<ITextureProcessingNode>* TextureNode = GraphInstance->GetOutputTextures().Find(TextureName);
		if (TextureNode)
		{
			TextureNode->Get().ComputeGraphHash(IdBuilder);
			TextureNode->Get().ComputeDataHash(Context, IdBuilder);
		}
	}

	return IdBuilder.Build();
}

FGuid FTextureSetCompiler::ComputeParameterDataId(const TSharedRef<IParameterProcessingNode> Parameter) const
{
	UE::DerivedData::FBuildVersionBuilder IdBuilder;
	IdBuilder << FString("TextureSetParameter_V0.6"); // Version string, bump this to invalidate everything
	IdBuilder << Args->UserKey; // Key for debugging, easily force rebuild
	Parameter->ComputeGraphHash(IdBuilder);
	Parameter->ComputeDataHash(Context, IdBuilder);
	return IdBuilder.Build();
}

void FTextureSetCompiler::LoadResources()
{
	// May need to create UObjects, so has to execute in game thread
	check(IsInGameThread());

	if (bResourcesLoaded)
		return;

	// Load all resources required by the graph
	for (const auto& [Name, TextureNode] : GraphInstance->GetOutputTextures())
		TextureNode->LoadResources(Context);

	for (const auto& [Name, ParameterNode] : GraphInstance->GetOutputParameters())
		ParameterNode->LoadResources(Context);

	bResourcesLoaded = true;
}

void FTextureSetCompiler::ConfigureTexture(FDerivedTexture& DerivedTexture, int Index) const
{
	FScopeLock Lock(DerivedTexture.TextureCS.Get());

	// Configure texture MUST be called only in the game thread.
	// Changing texture properties in a thread can cause a crash in the texture compiler.
	check(IsInGameThread());

	UTexture* Texture = DerivedTexture.Texture;

	const FTextureSetPackedTextureDef TextureDef = Args->PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = Args->PackingInfo.GetPackedTextureInfo(Index);

	// sRGB if possible
	Texture->SRGB = TextureInfo.HardwareSRGB;

	// Make sure the texture's compression settings are correct
	Texture->CompressionSettings = TextureDef.CompressionSettings;

	// Let the texture compression know if we don't need the alpha channel
	Texture->CompressionNoAlpha = TextureInfo.ChannelCount <= 3;

	Texture->VirtualTextureStreaming = TextureDef.bVirtualTextureStreaming;
	// Set the ID of the generated source to match the hash ID of this texture.
	// This will be used to recover the derived texture data from the DDC if possible.
	Texture->Source.SetId(GetTextureDataId(Index), true);

	// Important that this is set properly for streaming
	Texture->SetDeterministicLightingGuid();

	if (DerivedTexture.TextureState < EDerivedTextureState::Configured)
		DerivedTexture.TextureState = EDerivedTextureState::Configured;

}

void FTextureSetCompiler::InitializeTextureSource(FDerivedTexture& DerivedTexture, int Index) const
{
	check(bResourcesLoaded);
	check(IsInGameThread());

	FScopeLock Lock(DerivedTexture.TextureCS.Get());

	FTextureSource& Source = DerivedTexture.Texture->Source;

	check(DerivedTexture.TextureState >= EDerivedTextureState::Configured);

	if (DerivedTexture.TextureState >= EDerivedTextureState::SourceInitialized)
		return;

	const FTextureSetPackedTextureDef TextureDef = Args->PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = Args->PackingInfo.GetPackedTextureInfo(Index);
	int Width = 4;
	int Height = 4;
	int Slices = 1;
	int Mips = 1;
	float Ratio = 0.0f;

	const TMap<FName, TSharedRef<ITextureProcessingNode>>& OutputTextures = GraphInstance->GetOutputTextures();

	for (int c = 0; c < TextureInfo.ChannelCount; c++)
	{
		const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

		if (!OutputTextures.Contains(ChanelInfo.ProcessedTexture))
			continue;

		const TSharedRef<ITextureProcessingNode> OutputTexture = OutputTextures.FindChecked(ChanelInfo.ProcessedTexture);

		ITextureProcessingNode::FTextureDimension ChannelDimension = OutputTexture->GetTextureDimension();
		// Calculate the maximum size of all of our processed textures. We'll use this as our packed texture size.
		Width = FMath::Max(Width, ChannelDimension.Width);
		Height = FMath::Max(Height, ChannelDimension.Height);
		Slices = FMath::Max(Slices, ChannelDimension.Slices);

		if (Ratio == 0.0f)
		{
			Ratio = (float)ChannelDimension.Width / (float)ChannelDimension.Height;
		}
		else if (ChannelDimension.Width > 1 || ChannelDimension.Height > 1)
		{	
			// Verify that all processed textures have the same aspect ratio
			// Note: 1x1 textures do not factor in to this ratio check
			check(Ratio == ((float)ChannelDimension.Width / (float)ChannelDimension.Height));
		}
	};

	if (!Source.IsValid() || Source.GetSizeX() != Width || Source.GetSizeY() != Height || Source.GetNumSlices() != Slices || Source.GetNumMips() != Mips || Source.GetFormat() != TSF_RGBA32F)
	{
		FSharedBuffer ZeroLengthBuffer = FUniqueBuffer::Alloc(0).MoveToShared();
		Source.Init(Width, Height, Slices, Mips, TSF_RGBA32F, ZeroLengthBuffer);
		
		// Initializing source resets the ID, so put it back
		Source.SetId(GetTextureDataId(Index), true);
	}

	DerivedTexture.TextureState = EDerivedTextureState::SourceInitialized;
}

void FTextureSetCompiler::GenerateTextureSource(FDerivedTexture& DerivedTexture, int Index) const
{
	check(bResourcesLoaded);
	FScopeLock Lock(DerivedTexture.TextureCS.Get());

	check(DerivedTexture.TextureState >= EDerivedTextureState::SourceInitialized);

	if (DerivedTexture.TextureState == EDerivedTextureState::SourceGenerated)
		return; // Early out since we already have the source generated

#if BENCHMARK_TEXTURESET_COMPILATION
	const double BuildStartTime = FPlatformTime::Seconds();
	double SectionStartTime = BuildStartTime;
	FString DebugContext = DerivedTexture.Texture->GetName();
#endif

	const FTextureSetPackedTextureDef TextureDef = Args->PackingInfo.GetPackedTextureDef(Index);
	const FTextureSetPackedTextureInfo TextureInfo = Args->PackingInfo.GetPackedTextureInfo(Index);

	const TMap<FName, TSharedRef<ITextureProcessingNode>>& OutputTextures = GraphInstance->GetOutputTextures();

	for (int c = 0; c < TextureInfo.ChannelCount; c++)
	{
		const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

		if (!OutputTextures.Contains(ChanelInfo.ProcessedTexture))
			continue;

		const TSharedRef<ITextureProcessingNode> OutputTexture = OutputTextures.FindChecked(ChanelInfo.ProcessedTexture);

		OutputTexture->Initialize(*GraphInstance);
	}

	FVector4f RestoreMul = FVector4f::One();
	FVector4f RestoreAdd = FVector4f::Zero();

#if BENCHMARK_TEXTURESET_COMPILATION
	UE_LOG(LogTextureSet, Log, TEXT("%s Build: Initializing processing graph took %fs"), *DebugContext, FPlatformTime::Seconds() - SectionStartTime);
	SectionStartTime = FPlatformTime::Seconds();
#endif
 
	// TODO: Support textures sources of lower precision
	FTextureSource& Source = DerivedTexture.Texture->Source;
	const int Width = Source.GetSizeX();
	const int Height = Source.GetSizeY();
	const int Slices = Source.GetNumSlices();
	const FIntVector3 TextureSize(Width, Height, Slices);
	check(Source.GetFormat() == ETextureSourceFormat::TSF_RGBA32F);
	const uint8 PixelValueStride = 4;

	// Init with NewData == null is used to allocate space, which is then filled with LockMip
	Source.Init(Width, Height, Slices, 1, TSF_RGBA32F, nullptr);
	float* PixelValues = (float*)Source.LockMip(0);
	check(PixelValues != nullptr);

	#if BENCHMARK_TEXTURESET_COMPILATION
	UE_LOG(LogTextureSet, Log, TEXT("%s Build: Allocating source buffer took %fs"), *DebugContext, FPlatformTime::Seconds() - SectionStartTime);
	SectionStartTime = FPlatformTime::Seconds();
	#endif

	const FIntVector3 NumTiles = FIntVector3::DivideAndRoundUp(TextureSize, Args->TileSize);
	const int32 TotalTiles = NumTiles.X * NumTiles.Y * NumTiles.Z;

	for (uint8 c = 0; c < 4; c++) // Process each channel
	{
		const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

		if (c < TextureInfo.ChannelCount && OutputTextures.Contains(ChanelInfo.ProcessedTexture))
		{
			TSharedRef<ITextureProcessingNode> ProcessedTexture = OutputTextures.FindChecked(ChanelInfo.ProcessedTexture);
			ITextureProcessingNode::FTextureDimension ProcessedTextureDimension = ProcessedTexture->GetTextureDimension();

			check(ProcessedTextureDimension.Width <= Width);
			check(ProcessedTextureDimension.Height <= Height);

			if (ProcessedTextureDimension.Width < Width || ProcessedTextureDimension.Height < Height)
			{
				ProcessedTexture = MakeShared<FTextureOperatorEnlarge>(ProcessedTexture, Width, Height, Slices);
			}

			// Compute the tile for each channel
			for (int32 t = 0; t < TotalTiles; t++)
			{
				const FIntVector3 TileOffset(
					Args->TileSize.X * (t % NumTiles.X),
					Args->TileSize.Y * ((t / NumTiles.X) % NumTiles.Y),
					Args->TileSize.Z * (t / (NumTiles.X * NumTiles.Y))
				);
				check(TileOffset.Z < TextureSize.Z);

				const FIntVector3 TileSize = Args->TileSize.ComponentMin(TextureSize - TileOffset);
				check(TileSize.GetMin() > 0);

				const FIntVector3 DataStride = FTextureDataTileDesc::ComputeDataStrides(PixelValueStride, TextureSize);
				const int32 DataOffset = c + FTextureDataTileDesc::ComputeDataOffset(TileOffset, DataStride);

				FTextureDataTileDesc TileDesc(
					TextureSize,
					TileSize,
					TileOffset,
					DataStride,
					DataOffset
				);

				ProcessedTexture->ComputeChannel(ChanelInfo.ProessedTextureChannel, TileDesc, PixelValues);
			}

			#if BENCHMARK_TEXTURESET_COMPILATION
			UE_LOG(LogTextureSet, Log, TEXT("%s Build: Processing graph exectution for channel %i took %fs"), *DebugContext, c, FPlatformTime::Seconds() - SectionStartTime);
			SectionStartTime = FPlatformTime::Seconds();
			#endif

			// For encoding we don't allocate any data, so use a single tile that covers the whole image.
			FTextureDataTileDesc TileDesc(
				TextureSize,
				TextureSize,
				FIntVector3::ZeroValue,
				FTextureDataTileDesc::ComputeDataStrides(PixelValueStride, TextureSize),
				c
			);

			// Channel encoding (decoding happens in FTextureSetMaterialGraphBuilder::MakeTextureSamplerCustomNode)
			if (ChanelInfo.ChannelEncoding & (uint8)ETextureSetChannelEncoding::RangeCompression)
			{
				// Initialize the max and min pixel values so they will be overridden by the first pixel
				float Min = TNumericLimits<float>::Max();
				float Max = TNumericLimits<float>::Lowest();

				// Calculate the min and max values
				TileDesc.ForEachPixel([PixelValues, &Min, &Max](FTextureDataTileDesc::ForEachPixelContext& Context)
				{
					const float PixelValue = PixelValues[Context.DataIndex];
					Min = FMath::Min(Min, PixelValue);
					Max = FMath::Max(Max, PixelValue);
				});

				if (Min >= Max)
				{
					// Essentially ignore the texture at runtime and use the min value
					RestoreMul[c] = 0;
					RestoreAdd[c] = Min;
				}
				else
				{
					// Adjust the texture and set the constant values for decompression
					float CompressMul = 1.0f / (Max - Min);
					float CompressAdd = -Min * CompressMul;

					TileDesc.ForEachPixel([PixelValues, CompressMul, CompressAdd](FTextureDataTileDesc::ForEachPixelContext& Context)
					{
						PixelValues[Context.DataIndex] = PixelValues[Context.DataIndex] * CompressMul + CompressAdd;
					});

					RestoreMul[c] = Max - Min;
					RestoreAdd[c] = Min;

					#if BENCHMARK_TEXTURESET_COMPILATION
					UE_LOG(LogTextureSet, Log, TEXT("%s Build: Range compression of channel %i took %fs"), *DebugContext, c, FPlatformTime::Seconds() - SectionStartTime);
					SectionStartTime = FPlatformTime::Seconds();
					#endif
				}
			}

			if ((ChanelInfo.ChannelEncoding & (uint8)ETextureSetChannelEncoding::SRGB) && (!TextureInfo.HardwareSRGB || c >= 3))
			{
				TileDesc.ForEachPixel([PixelValues](FTextureDataTileDesc::ForEachPixelContext& Context)
				{
					PixelValues[Context.DataIndex] = FMath::Pow(PixelValues[Context.DataIndex], 1.0f / 2.2f);
				});

				#if BENCHMARK_TEXTURESET_COMPILATION
				UE_LOG(LogTextureSet, Log, TEXT("%s Build: SRGB adjustment of channel %i took %fs"), *DebugContext, c, FPlatformTime::Seconds() - SectionStartTime);
				SectionStartTime = FPlatformTime::Seconds();
				#endif
			}

		}
		else
		{
			FTextureDataTileDesc TileDesc(
				TextureSize,
				TextureSize,
				FIntVector3::ZeroValue,
				FTextureDataTileDesc::ComputeDataStrides(PixelValueStride, TextureSize),
				c
			);

			if (c < 3)
			{
				TileDesc.ForEachPixel([PixelValues](FTextureDataTileDesc::ForEachPixelContext& Context)
				{
					PixelValues[Context.DataIndex] = 0.0f; // Fill RGB with black
				});
			}
			else
			{
				TileDesc.ForEachPixel([PixelValues](FTextureDataTileDesc::ForEachPixelContext& Context)
				{
					PixelValues[Context.DataIndex] = 1.0f; // Fill Alpha with white
				});
			}

			#if BENCHMARK_TEXTURESET_COMPILATION
				UE_LOG(LogTextureSet, Log, TEXT("%s Build: Filling channel %i with blank data took %fs"), *DebugContext, c, FPlatformTime::Seconds() - SectionStartTime);
				SectionStartTime = FPlatformTime::Seconds();
			#endif
		}
	};

	Source.UnlockMip(0);

	// UnlockMip causes GUID to be set from a hash, so force it back to the one we want to use
	Source.SetId(GetTextureDataId(Index), true);

	FDerivedTextureData& Data = DerivedTexture.Data;
	Data.Id = GetTextureDataId(Index);

	if (RestoreMul != FVector4f::One() || RestoreAdd != FVector4f::Zero())
	{
		Data.TextureParameters.Add(TextureInfo.RangeCompressMulName, RestoreMul);
		Data.TextureParameters.Add(TextureInfo.RangeCompressAddName, RestoreAdd);
	}

#if BENCHMARK_TEXTURESET_COMPILATION
	const double BuildEndTime = FPlatformTime::Seconds();
	UE_LOG(LogTextureSet, Log, TEXT("%s: texture generation took %fs"), *DebugContext, BuildEndTime - BuildStartTime);
#endif

	DerivedTexture.TextureState = EDerivedTextureState::SourceGenerated;
}

void FTextureSetCompiler::FreeTextureSource(FDerivedTexture& DerivedTexture, int Index) const
{
	check(DerivedTexture.TextureState >= EDerivedTextureState::SourceInitialized);

	if (DerivedTexture.TextureState == EDerivedTextureState::SourceInitialized)
		return;

	FTextureSource& Source = DerivedTexture.Texture->Source;
	FSharedBuffer ZeroLengthBuffer = FUniqueBuffer::Alloc(0).MoveToShared();
	DerivedTexture.Texture->Source.Init(Source.GetSizeX(), Source.GetSizeY(), Source.GetNumSlices(), Source.GetNumMips(), Source.GetFormat(), ZeroLengthBuffer);
		
	// Initializing source resets the ID, so put it back
	Source.SetId(GetTextureDataId(Index), true);

	DerivedTexture.TextureState = EDerivedTextureState::SourceInitialized;
}

FDerivedParameterData FTextureSetCompiler::BuildParameterData(FName Name) const
{
	check(bResourcesLoaded);
	TSharedRef<IParameterProcessingNode> Parameter = GraphInstance->GetOutputParameters().FindChecked(Name);

	Parameter->Initialize(*GraphInstance);

	FDerivedParameterData ParameterData;
	ParameterData.Value = Parameter->GetValue();
	ParameterData.Id = GetParameterDataId(Name);
	return ParameterData;
}
