// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetCompiler.h"

#include "Async/ParallelFor.h"
#include "DerivedDataBuildVersion.h"
#include "DerivedDataCacheInterface.h"
#include "ProcessingNodes/TextureOperatorEnlarge.h"
#include "ProcessingNodes/TextureOperatorMipChain.h"
#include "ProcessingNodes/TextureOperatorLodBias.h"
#include "TextureSetDerivedData.h"
#include "TextureSetsHelpers.h"
#include "ImageCoreUtils.h"

#define BENCHMARK_TEXTURESET_COMPILATION 1

FTextureSetCompiler::FTextureSetCompiler(TSharedRef<const FTextureSetCompilerArgs> Args)
	: Args(Args)
	, bPrepared(false)
{
	check(IsInGameThread());

	// TODO: Get module list out of module info
	GraphInstance = MakeShared<FTextureSetProcessingGraph>(Args->ModuleInfo.GetModules());

	Context.SourceTextures = Args->SourceTextures;
	Context.AssetParams = Args->AssetParams;
	Context.Graph = GraphInstance;

	CachedDerivedTextureIds.SetNum(Args->PackingInfo.NumPackedTextures());

	Prepare();
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
	{
		// Only valid to calculate in the game thread, as hashing reads UObjects.
		check(IsInGameThread())

		CachedDerivedTextureIds[Index] = ComputeTextureDataId(Index);
	}

	return CachedDerivedTextureIds[Index];
}

FGuid FTextureSetCompiler::GetParameterDataId(FName Name) const
{
	if (!CachedParameterIds.Contains(Name))
	{
		// Only valid to calculate in the game thread, as hashing reads UObjects.
		check(IsInGameThread())

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
	UE::DerivedData::FBuildVersionBuilder IdBuilder;
	IdBuilder << FString("TextureSetDerivedTexture_V0.25"); // Version string, bump this to invalidate everything
	IdBuilder << Args->UserKey; // Key for debugging, easily force rebuild
	IdBuilder << GetTypeHash(Args->PackingInfo.GetPackedTextureDef(PackedTextureIndex));

	for (int c = 0; c < 4; c++)
	{
		IdBuilder << c;

		TSharedPtr<ITextureProcessingNode> Node = PackedTextureNodes[PackedTextureIndex][c];
		if (Node.IsValid())
		{
			Node->ComputeGraphHash(IdBuilder);
			Node->ComputeDataHash(Context, IdBuilder);
		}
	}

	return IdBuilder.Build();
}

FGuid FTextureSetCompiler::ComputeParameterDataId(const TSharedRef<IParameterProcessingNode> Parameter) const
{
	UE::DerivedData::FBuildVersionBuilder IdBuilder;
	IdBuilder << FString("TextureSetParameter_V0.8"); // Version string, bump this to invalidate everything
	IdBuilder << Args->UserKey; // Key for debugging, easily force rebuild
	Parameter->ComputeGraphHash(IdBuilder);
	Parameter->ComputeDataHash(Context, IdBuilder);
	return IdBuilder.Build();
}

void FTextureSetCompiler::Prepare()
{
	// May needs to access UObjects, so has to execute in game thread
	check(IsInGameThread());

	if (bPrepared)
		return;

	const TMap<FName, TSharedRef<ITextureProcessingNode>>& GraphOutputTextures = GraphInstance->GetOutputTextures();
	TMap<FName, TSharedRef<ITextureProcessingNode>> MippedOutputTextures;

	for (auto& [Name, Node] : GraphOutputTextures)
	{
		Node->Prepare(Context);

		const ITextureProcessingNode::FTextureDimension SourceDim = Node->GetTextureDimension();
		const int32 ExpectedMips = FImageCoreUtils::GetMipCountFromDimensions(SourceDim.Width, SourceDim.Height, SourceDim.Slices, Node->GetTextureDef().IsVolume());
		
		if (SourceDim.Mips < ExpectedMips) // Add mip chain to output texture if it doesn't already have enough mips
		{
			MippedOutputTextures.Emplace(Name, MakeShared<FTextureOperatorMipChain>(Node));
		}
		else // Otherwise we can use the node directly
		{
			MippedOutputTextures.Emplace(Name, Node);
		}
	}

	PackedTextureDims.SetNum(Args->PackingInfo.NumPackedTextures());
	PackedTextureNodes.SetNum(Args->PackingInfo.NumPackedTextures());

	for (int i = 0; i < Args->PackingInfo.NumPackedTextures(); i++)
	{
		const FTextureSetPackedTextureInfo TextureInfo = Args->PackingInfo.GetPackedTextureInfo(i);
		float Ratio = 0.0f;

		ITextureProcessingNode::FTextureDimension& TextureDim = PackedTextureDims[i];

		// Iterate all channels to determine the largest size to be packed, which will be used as the packed texture size.
		for (int c = 0; c < TextureInfo.ChannelCount; c++)
		{
			const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

			if (!MippedOutputTextures.Contains(ChanelInfo.ProcessedTexture))
				continue;

			const TSharedRef<ITextureProcessingNode> OutputTexture = MippedOutputTextures.FindChecked(ChanelInfo.ProcessedTexture);
			OutputTexture->Prepare(Context);
			const ITextureProcessingNode::FTextureDimension ChannelDim = OutputTexture->GetTextureDimension();

			// Calculate the maximum size of all of our processed textures. We'll use this as our packed texture size.
			TextureDim.Width = FMath::Max(TextureDim.Width, ChannelDim.Width);
			TextureDim.Height = FMath::Max(TextureDim.Height, ChannelDim.Height);
			TextureDim.Slices = FMath::Max(TextureDim.Slices, ChannelDim.Slices);

			// Note: 1x1 textures do not factor in to this ratio check
			if (ChannelDim.Width > 1 || ChannelDim.Height > 1)
			{
				if (Ratio == 0.0f) // If ratio has not been set, set it now
					Ratio = (float)ChannelDim.Width / (float)ChannelDim.Height;
				else // Otherwise, verify that all channels in this texture have the same aspect ratio
					check(Ratio == ((float)ChannelDim.Width / (float)ChannelDim.Height));
			}
		};

		const bool IsVolume = (TextureInfo.Flags & (uint8)ETextureSetTextureFlags::Array) == 0;
		TextureDim.Mips = FImageCoreUtils::GetMipCountFromDimensions(TextureDim.Width, TextureDim.Height, TextureDim.Slices, IsVolume);

		for (int c = 0; c < TextureInfo.ChannelCount; c++)
		{
			const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

			if (!MippedOutputTextures.Contains(ChanelInfo.ProcessedTexture))
			{
				PackedTextureNodes[i][c] = nullptr;
				continue;
			}

			TSharedRef<ITextureProcessingNode> OutputTexture = MippedOutputTextures.FindChecked(ChanelInfo.ProcessedTexture);
			const ITextureProcessingNode::FTextureDimension ChannelDim = OutputTexture->GetTextureDimension();

			// If textures have a different number of mips, then add a bias to adjust
			if (TextureDim.Mips != ChannelDim.Mips)
				OutputTexture = MakeShared<FTextureOperatorLodBias>(OutputTexture, ChannelDim.Mips - TextureDim.Mips);

			// Prepare the texture node
			OutputTexture->Prepare(Context);
			PackedTextureNodes[i][c] = OutputTexture.ToSharedPtr();
		}
	}

	for (const auto& [Name, ParameterNode] : GraphInstance->GetOutputParameters())
	{
		// Prepare parameter node
		ParameterNode->Prepare(Context);
	}

	// Prime all data IDs so they can be accessed from threads
	for (int i = 0; i < Args->PackingInfo.NumPackedTextures(); i++)
		GetTextureDataId(i);

	for (const auto& [Name, ParameterNode] : GraphInstance->GetOutputParameters())
		GetParameterDataId(Name);

	bPrepared = true;
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

	Texture->MipGenSettings = TMGS_LeaveExistingMips;

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
	check(bPrepared);
	check(IsInGameThread());

	FScopeLock Lock(DerivedTexture.TextureCS.Get());

	FTextureSource& Source = DerivedTexture.Texture->Source;

	check(DerivedTexture.TextureState >= EDerivedTextureState::Configured);

	if (DerivedTexture.TextureState >= EDerivedTextureState::SourceInitialized)
		return;

	const ITextureProcessingNode::FTextureDimension& TextureDim = PackedTextureDims[Index];

	if (!Source.IsValid() 
		|| Source.GetSizeX() != TextureDim.Width 
		|| Source.GetSizeY() != TextureDim.Height 
		|| Source.GetNumSlices() != TextureDim.Slices 
		|| Source.GetNumMips() != TextureDim.Mips 
		|| Source.GetFormat() != TSF_RGBA32F)
	{
		FSharedBuffer ZeroLengthBuffer = FUniqueBuffer::Alloc(0).MoveToShared();
		Source.Init(TextureDim.Width, TextureDim.Height, TextureDim.Slices, TextureDim.Mips, TSF_RGBA32F, ZeroLengthBuffer);
		
		// Initializing source resets the ID, so put it back
		Source.SetId(GetTextureDataId(Index), true);
	}

	DerivedTexture.TextureState = EDerivedTextureState::SourceInitialized;
}

void FTextureSetCompiler::GenerateTextureSource(FDerivedTexture& DerivedTexture, int Index) const
{
	check(bPrepared);
	FScopeLock Lock(DerivedTexture.TextureCS.Get());

	check(DerivedTexture.TextureState >= EDerivedTextureState::SourceInitialized);

	if (DerivedTexture.TextureState == EDerivedTextureState::SourceGenerated)
		return; // Early out since we already have the source generated

#if BENCHMARK_TEXTURESET_COMPILATION
	const double BuildStartTime = FPlatformTime::Seconds();
	double SectionStartTime = BuildStartTime;
	FString DebugContext = DerivedTexture.Texture->GetName();
#endif

	const FTextureSetPackedTextureInfo TextureInfo = Args->PackingInfo.GetPackedTextureInfo(Index);

	const TMap<FName, TSharedRef<ITextureProcessingNode>>& OutputTextures = GraphInstance->GetOutputTextures();

	for (int c = 0; c < PackedTextureNodes[Index].Num(); c++)
	{
		const TSharedPtr<ITextureProcessingNode> OutputTexture = PackedTextureNodes[Index][c];

		if (OutputTexture.IsValid())
			OutputTexture->Cache();
	}

	FVector4f RestoreMul = FVector4f::One();
	FVector4f RestoreAdd = FVector4f::Zero();

	#if BENCHMARK_TEXTURESET_COMPILATION
		UE_LOG(LogTextureSet, Log, TEXT("%s Build: Caching processing graph took %fs"), *DebugContext, FPlatformTime::Seconds() - SectionStartTime);
		SectionStartTime = FPlatformTime::Seconds();
	#endif
 
	// TODO: Support textures sources of lower precision
	FTextureSource& Source = DerivedTexture.Texture->Source;
	const int Width = Source.GetSizeX();
	const int Height = Source.GetSizeY();
	const int Slices = Source.GetNumSlices();
	const int Mips = Source.GetNumMips();
	check(Source.GetFormat() == ETextureSourceFormat::TSF_RGBA32F);
	const uint8 PixelValueStride = 4;

	// Init with NewData == null is used to allocate space, which is then filled with LockMip
	Source.Init(Width, Height, Slices, Mips, TSF_RGBA32F, nullptr);
	
	// Lock all mips for writing, and get pointers to pixel data
	TArray<float*> PixelValues;
	PixelValues.SetNumUninitialized(Mips, false);
	TArray<FImageInfo> MipInfo;
	MipInfo.SetNumUninitialized(Mips, false);
	for (int m = 0; m < Mips; m++)
	{
		PixelValues[m] = (float*)Source.LockMip(m);
		check(PixelValues[m] != nullptr);

		Source.GetMipImageInfo(MipInfo[m], 0, 0, m);
	}

	#if BENCHMARK_TEXTURESET_COMPILATION
		UE_LOG(LogTextureSet, Log, TEXT("%s Build: Allocating source buffer took %fs"), *DebugContext, FPlatformTime::Seconds() - SectionStartTime);
		SectionStartTime = FPlatformTime::Seconds();
	#endif

	// Process each channel
	for (uint8 c = 0; c < PixelValueStride; c++)
	{
		const auto& ChanelInfo = TextureInfo.ChannelInfo[c];

		if (c < TextureInfo.ChannelCount && PackedTextureNodes[Index][c].IsValid())
		{
			TSharedPtr<ITextureProcessingNode> ProcessedTexture = PackedTextureNodes[Index][c];
			ITextureProcessingNode::FTextureDimension ProcessedTextureDimension = PackedTextureDims[Index];

			check(ProcessedTextureDimension.Width == Width);
			check(ProcessedTextureDimension.Height == Height);

			// Process each mip
			for (int m = 0; m < Mips; m++)
			{
				check(m < ProcessedTextureDimension.Mips);

				const FIntVector3 MipSize = FIntVector3(MipInfo[m].SizeX, MipInfo[m].SizeY, MipInfo[m].NumSlices);
				const FIntVector3 NumTiles = FIntVector3::DivideAndRoundUp(MipSize, Args->TileSize);
				const int32 TotalTiles = NumTiles.X * NumTiles.Y * NumTiles.Z;

				// Compute the tile
				for (int32 t = 0; t < TotalTiles; t++)
				{
					const FIntVector3 TileOffset(
						Args->TileSize.X * (t % NumTiles.X),
						Args->TileSize.Y * ((t / NumTiles.X) % NumTiles.Y),
						Args->TileSize.Z * (t / (NumTiles.X * NumTiles.Y))
					);
					check(TileOffset.Z < MipSize.Z);

					const FIntVector3 TileSize = Args->TileSize.ComponentMin(MipSize - TileOffset);
					check(TileSize.GetMin() > 0);

					const FIntVector3 DataStride = FTextureDataTileDesc::ComputeDataStrides(PixelValueStride, MipSize);
					const int32 DataOffset = c + FTextureDataTileDesc::ComputeDataOffset(TileOffset, DataStride);

					FTextureDataTileDesc TileDesc(
						MipSize,
						TileSize,
						TileOffset,
						DataStride,
						DataOffset
					);

					ProcessedTexture->WriteChannel(ChanelInfo.ProessedTextureChannel, m, TileDesc, PixelValues[m]);
				}
			}

			#if BENCHMARK_TEXTURESET_COMPILATION
				UE_LOG(LogTextureSet, Log, TEXT("%s Build: Processing graph exectution for channel %i took %fs"), *DebugContext, c, FPlatformTime::Seconds() - SectionStartTime);
				SectionStartTime = FPlatformTime::Seconds();
			#endif

			// Channel encoding (decoding happens in FTextureSetSampleFunctionBuilder::BuildTextureDecodeNode)

			auto GetMipEncodingTile = [&MipInfo](int c, int m)
			{
				const FIntVector3 MipSize = FIntVector3(MipInfo[m].SizeX, MipInfo[m].SizeY, MipInfo[m].NumSlices);

				// For encoding we don't allocate any data, so use a single tile that covers the whole image.
				return FTextureDataTileDesc(
					MipSize,
					MipSize,
					FIntVector3::ZeroValue,
					FTextureDataTileDesc::ComputeDataStrides(PixelValueStride, MipSize),
					c
				);
			};

			// Encode range compression
			if (ChanelInfo.ChannelEncoding & (uint8)ETextureSetChannelEncoding::RangeCompression)
			{
				// Initialize the max and min pixel values so they will be overridden by the first pixel
				float Min = TNumericLimits<float>::Max();
				float Max = TNumericLimits<float>::Lowest();

				for (int m = 0; m < Mips; m++)
				{
					// Calculate the min and max values
					GetMipEncodingTile(c,m).ForEachPixel([PixelValues, m, &Min, &Max](FTextureDataTileDesc::ForEachPixelContext& Context)
					{
						const float PixelValue = PixelValues[m][Context.DataIndex];
						Min = FMath::Min(Min, PixelValue);
						Max = FMath::Max(Max, PixelValue);
					});
				}

				if (Min == Max)
				{
					// Essentially ignore the texture at runtime and use the min value
					RestoreMul[c] = 0;
					RestoreAdd[c] = Min;
				}
				else
				{
					check(Min < Max);

					// Adjust the texture and set the constant values for decompression
					float CompressMul = 1.0f / (Max - Min);
					float CompressAdd = -Min * CompressMul;

					for (int m = 0; m < Mips; m++)
					{
						GetMipEncodingTile(c,m).ForEachPixel([PixelValues, m, CompressMul, CompressAdd](FTextureDataTileDesc::ForEachPixelContext& Context)
						{
							PixelValues[m][Context.DataIndex] = PixelValues[m][Context.DataIndex] * CompressMul + CompressAdd;
						});
					}

					RestoreMul[c] = Max - Min;
					RestoreAdd[c] = Min;
				}

				#if BENCHMARK_TEXTURESET_COMPILATION
					UE_LOG(LogTextureSet, Log, TEXT("%s Build: Range compression of channel %i took %fs"), *DebugContext, c, FPlatformTime::Seconds() - SectionStartTime);
					SectionStartTime = FPlatformTime::Seconds();
				#endif
			}

			// Encode sRGB
			if ((ChanelInfo.ChannelEncoding & (uint8)ETextureSetChannelEncoding::SRGB) && (!TextureInfo.HardwareSRGB || c >= 3))
			{
				for (int m = 0; m < Mips; m++)
				{
					GetMipEncodingTile(c,m).ForEachPixel([PixelValues, m](FTextureDataTileDesc::ForEachPixelContext& Context)
					{
						PixelValues[m][Context.DataIndex] = FMath::Pow(PixelValues[m][Context.DataIndex], 1.0f / 2.2f);
					});
				}

				#if BENCHMARK_TEXTURESET_COMPILATION
					UE_LOG(LogTextureSet, Log, TEXT("%s Build: sRGB encoding of channel %i took %fs"), *DebugContext, c, FPlatformTime::Seconds() - SectionStartTime);
					SectionStartTime = FPlatformTime::Seconds();
				#endif
			}

		}
		else // Channel not mapped to a processing graph output
		{
			for (int m = 0; m < Mips; m++) // Fill mips with blank data
			{
				const FIntVector3 MipSize = FIntVector3(MipInfo[m].SizeX, MipInfo[m].SizeY, MipInfo[m].NumSlices);
				FTextureDataTileDesc TileDesc(
					MipSize,
					MipSize,
					FIntVector3::ZeroValue,
					FTextureDataTileDesc::ComputeDataStrides(PixelValueStride, MipSize),
					c
				);

				if (c < 3)
				{
					TileDesc.ForEachPixel([PixelValues, m](FTextureDataTileDesc::ForEachPixelContext& Context)
					{
						PixelValues[m][Context.DataIndex] = 0.0f; // Fill RGB with black
					});
				}
				else
				{
					TileDesc.ForEachPixel([PixelValues, m](FTextureDataTileDesc::ForEachPixelContext& Context)
					{
						PixelValues[m][Context.DataIndex] = 1.0f; // Fill Alpha with white
					});
				}
			}

			#if BENCHMARK_TEXTURESET_COMPILATION
				UE_LOG(LogTextureSet, Log, TEXT("%s Build: Filling channel %i with blank data took %fs"), *DebugContext, c, FPlatformTime::Seconds() - SectionStartTime);
				SectionStartTime = FPlatformTime::Seconds();
			#endif
		}
	};

	for (int m = 0; m < Mips; m++)
	{
		Source.UnlockMip(m);
	}

	// UnlockMip causes GUID to be set from a hash, so force it back to the one we want to use
	Source.SetId(GetTextureDataId(Index), true);

	DerivedTexture.Data.Id = GetTextureDataId(Index);

	if (RestoreMul != FVector4f::One() || RestoreAdd != FVector4f::Zero())
	{
		DerivedTexture.Data.TextureParameters.Add(TextureInfo.RangeCompressMulName, RestoreMul);
		DerivedTexture.Data.TextureParameters.Add(TextureInfo.RangeCompressAddName, RestoreAdd);
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
	check(bPrepared);
	TSharedRef<IParameterProcessingNode> Parameter = GraphInstance->GetOutputParameters().FindChecked(Name);

	Parameter->Cache();

	FDerivedParameterData ParameterData;
	ParameterData.Value = Parameter->GetValue();
	ParameterData.Id = GetParameterDataId(Name);
	return ParameterData;
}
