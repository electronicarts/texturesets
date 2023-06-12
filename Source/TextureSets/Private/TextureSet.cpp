// (c) Electronic Arts. All Rights Reserved.

#include "TextureSet.h"

#include "TextureSetDefinition.h"
#include "TextureSetModule.h"
#include "TextureSetModifiersAssetUserData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/ObjectSaveContext.h"
#include "Engine/TextureDefines.h"
#include "ImageUtils.h"

static TAutoConsoleVariable<int32> CVarTextureSetFreeImmediateImages(
	TEXT("r.TextureSet.FreeImmediateImages"),
	1,
	TEXT("If enabled, Texture Set will free the memory of immediate images after resource caching"),
	ECVF_Default);

UTextureSet::UTextureSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{}

void UTextureSet::PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext)
{
	if (ObjectSaveContext.IsCooking())
		return;

	UpdateCookedTextures();
}

void UTextureSet::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
#if WITH_EDITOR	
	if (!IsValid(Definition))
		return;

	if (ObjectSaveContext.IsCooking())
		return;

	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();

	for (int32 PackedTextureIndex = 0; PackedTextureIndex < PackingInfo.NumPackedTextures(); PackedTextureIndex++)
	{
		UTexture* CookedTexture = CookedTextures[PackedTextureIndex].IsValid() ? CookedTextures[PackedTextureIndex].Get() : CookedTextures[PackedTextureIndex].LoadSynchronous();
		CookedTexture->UpdateResource();
		//CookedTexture->PostEditChange();
		//CookedTexture->Modify();
	}
#endif
}

void UTextureSet::UpdateCookedTextures()
{
#if WITH_EDITOR	
	if (!IsValid(Definition))
		return;

	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();

	{
		FScopeLock Lock(&TextureSetCS);

		TArray<FString> NewPackedTextureKeys = ComputePackedTextureKeys();
		if (NewPackedTextureKeys != PackedTextureKeys)
		{
			IsTextureSetProcessed = false;
			PackedTextureKeys = NewPackedTextureKeys;
		}
	}

	if (CookedTextures.Num() > PackingInfo.NumPackedTextures())
	{
		CookedTextures.SetNum(PackingInfo.NumPackedTextures());

		// Garbage collection will destroy the unused cooked textures when all references from material instance are removed 
	}
	else
	{
		while (CookedTextures.Num() < PackingInfo.NumPackedTextures())
		{
			const FTextureSetPackedTextureDef& Def = PackingInfo.GetPackedTextureDef(CookedTextures.Num());
			const auto& Info = PackingInfo.GetPackedTextureInfo(CookedTextures.Num());

			FString TextureName = GetName() + "_CookedTexture_" + FString::FromInt(CookedTextures.Num());
			const int CookedTexturesize = 4;

			FLinearColor SourceColor = FLinearColor(Info.DefaultValue);

			// TODO linearcolor
			TArray<FColor> DefaultData;

			for (int i = 0; i < (CookedTexturesize * CookedTexturesize); i++)
				DefaultData.Add(SourceColor.ToFColor(false));

			EObjectFlags Flags = RF_Public | RF_Standalone;

			FCreateTexture2DParameters Params;
			Params.bUseAlpha = Def.UsedChannels() >= 4;
			Params.CompressionSettings = TextureCompressionSettings::TC_MAX; // to make sure later this asset is marked as modified
			Params.bDeferCompression = false;
			Params.bSRGB = Def.GetHardwareSRGBEnabled();
			Params.bVirtualTexture = false;

			UTexture2D* CookedTexture = FindObject<UTexture2D>(this, *TextureName);
			UTextureSetModifiersAssetUserData* TextureModifier = nullptr;
			if (CookedTexture != nullptr)
			{
				check(CookedTexture->Source.IsValid());
				TextureModifier = CookedTexture->GetAssetUserData< UTextureSetModifiersAssetUserData >();
			}
			else
			{
				CookedTexture = FImageUtils::CreateTexture2D(4, 4, DefaultData, this, TextureName, Flags, Params);
			}

			if (TextureModifier == nullptr)
			{
				FString AssetUserDataName = GetName() + "_CookedTexture_" + FString::FromInt(CookedTextures.Num()) + "_AssetUserData";
				TextureModifier = NewObject<UTextureSetModifiersAssetUserData>(CookedTexture, FName(*AssetUserDataName), Flags);
				CookedTexture->AddAssetUserData(TextureModifier);
			}
			TextureModifier->TextureSet = this;
			TextureModifier->PackedTextureDefIndex = CookedTextures.Num();

			CookedTextures.Add(CookedTexture);
		}
	}

	for (int32 PackedTextureIndex = 0; PackedTextureIndex < PackingInfo.NumPackedTextures(); PackedTextureIndex++)
	{
		UTexture* CookedTexture = CookedTextures[PackedTextureIndex].IsValid() ? CookedTextures[PackedTextureIndex].Get() : CookedTextures[PackedTextureIndex].LoadSynchronous();

		const FTextureSetPackedTextureDef& Def = PackingInfo.GetPackedTextureDef(PackedTextureIndex);

		if (CookedTexture->CompressionSettings != Def.CompressionSettings)
		{
			CookedTexture->CompressionSettings = Def.CompressionSettings;
			CookedTexture->Modify();
		}
		//CookedTexture->Modify();
		//CookedTexture->UpdateResource();
	}

#endif
}

void UTextureSet::PostLoad()
{
	Super::PostLoad();
	FixupData();
	UpdateCookedTextures();
}

#if WITH_EDITOR
void UTextureSet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName ChangedPropName = PropertyChangedEvent.GetPropertyName();

	if (ChangedPropName == GET_MEMBER_NAME_CHECKED(UTextureSet, Definition)
		&& PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		FixupData();
	}
}
#endif

void UTextureSet::FixupData()
{
#if WITH_EDITOR
	// Only fixup the data if we have a valid definition. Otherwise leave it as-is so it's there for when we do.
	if (IsValid(Definition))
	{
		// Source Textures
		TMap<FName, TObjectPtr<UTexture>> NewSourceTextures;

		for (auto& TextureInfo : Definition->GetSharedInfo().GetSourceTextures())
		{
			TObjectPtr<UTexture>* OldTexture = SourceTextures.Find(TextureInfo.Name);
			NewSourceTextures.Add(TextureInfo.Name, (OldTexture != nullptr) ? *OldTexture : nullptr);
		}

		SourceTextures = NewSourceTextures;

		// Asset Params
		TArray<TSubclassOf<UTextureSetAssetParams>> RequiredAssetParamClasses = Definition->GetRequiredAssetParamClasses();
		TArray<TSubclassOf<UTextureSetAssetParams>> ExistingAssetParamClasses;
		
		// Remove un-needed asset params
		for (int i = 0; i < AssetParams.Num(); i++)
		{
			UTextureSetAssetParams* AssetParam = AssetParams[i];
			if (!RequiredAssetParamClasses.Contains(AssetParam->StaticClass()))
			{
				AssetParams.RemoveAt(i);
				i--;
			}
			else
			{
				ExistingAssetParamClasses.Add(AssetParam->StaticClass());
			}
		}
		
		// Add missing asset params
		for (TSubclassOf<UTextureSetAssetParams> SampleParamClass : RequiredAssetParamClasses)
		{
			if (!ExistingAssetParamClasses.Contains(SampleParamClass))
			{
				AssetParams.Add(NewObject<UTextureSetAssetParams>(this, SampleParamClass));
			}
		}
	}
#endif
}

FString UTextureSet::GetPackedTextureDefKey(int PackedTextureDefIndex)
{
	if (!IsValid(Definition))
		return TEXT("INVALID_TEXTURE_SET_DEFINITION");

	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();
	if (PackedTextureDefIndex < PackingInfo.NumPackedTextures())
	{
		// all data hash keys start with a global version tracking format changes
		FString PackedTextureDataKey("TEXTURE_SET_PACKING_VER1_");

		// track the packing rule and setting
		PackedTextureDataKey += Definition->GetPackedTextureDefKey(PackedTextureDefIndex);
		PackedTextureDataKey += "_";

		// track the source data
		const FTextureSetPackedTextureDef& PackedTextureDef = PackingInfo.GetPackedTextureDef(PackedTextureDefIndex);
		const TArray<FString>& SourcesWithoutChannel = PackedTextureDef.GetSourcesWithoutChannel();
		for (const FString& Source : SourcesWithoutChannel)
		{
			TObjectPtr<UTexture>* SourceTexture = SourceTextures.Find(FName(*Source));
			if ((SourceTexture != nullptr) && (SourceTexture->Get() != nullptr))
			{
				PackedTextureDataKey += Source;
				PackedTextureDataKey += "<";
				PackedTextureDataKey += SourceTexture->Get()->Source.GetId().ToString();
				PackedTextureDataKey += ">_";
			}
		}

		// Tag for debugging, easily force rebuild
		if (!AssetTag.IsEmpty())
			PackedTextureDataKey += AssetTag;

		//PackedTextureDataKey.ToLowerInline();
		return PackedTextureDataKey;
	}
	else
	{
		return TEXT("INVALID_PACKED_TEXTURE_ASSET_INDEX");
	}
}

TArray<FString> UTextureSet::ComputePackedTextureKeys()
{
	TArray<FString> NewPackedTextureKeys;

	if (!IsValid(Definition))
		return NewPackedTextureKeys;

	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();
	for (int32 i = 0; i < PackingInfo.NumPackedTextures(); i++)
	{
		NewPackedTextureKeys.Add(GetPackedTextureDefKey(i));
	}

	return NewPackedTextureKeys;
}

int32 GetBytesPerChannel(ERawImageFormat::Type Format)
{
	int32 BytesPerChannel = 0;
	switch (Format)
	{
	case ERawImageFormat::G8:
	case ERawImageFormat::BGRA8:
	case ERawImageFormat::BGRE8:
		BytesPerChannel = 1;
		break;

	case ERawImageFormat::G16:
	case ERawImageFormat::R16F:
	case ERawImageFormat::RGBA16:
	case ERawImageFormat::RGBA16F:
		BytesPerChannel = 2;
		break;

	case ERawImageFormat::R32F:
	case ERawImageFormat::RGBA32F:
		BytesPerChannel = 4;
		break;

	default:
		check(0);
		break;
	}
	return BytesPerChannel;
}


void UTextureSet::ModifyTextureSource(int PackedTextureDefIndex, UTexture* TextureAsset)
{
	if (!IsValid(Definition))
		return;

	if ((PackedTextureDefIndex >= CookedTextures.Num()) || (CookedTextures[PackedTextureDefIndex].IsNull()))
	{
		checkf(false, TEXT("Invalid Index of Packed Texture within TextureSet."));
		return;
	}

	UTexture* CookedTexture = CookedTextures[PackedTextureDefIndex].Get();
	if (CookedTexture != TextureAsset)
	{
		checkf(false, TEXT("Invalid Cooked Texture within TextureSet."));
		return;
	}
	const TextureSetPackingInfo& PackingInfo = Definition->GetPackingInfo();
	const FTextureSetPackedTextureDef& TextureSetPackedTextureDef = PackingInfo.GetPackedTextureDef(PackedTextureDefIndex);
	if (TextureSetPackedTextureDef.UsedChannels() == 0)
	{
		checkf(false, TEXT("Invalid Packing within TextureSetDefinition."));
		return;
	}

	// Preprocessing
	{
		FScopeLock Lock(&TextureSetCS);

		TArray<FString> NewPackedTextureKeys = ComputePackedTextureKeys();
		if (NewPackedTextureKeys != PackedTextureKeys)
		{
			IsTextureSetProcessed = false;
			PackedTextureKeys = NewPackedTextureKeys;
		}

		if (!IsTextureSetProcessed)
		{
			SourceRawImages.Empty();

			int32 SizeX = -1;
			int32 SizeY = -1;
			int32 NumSlices = -1;

			// Decompress source textures
			{
				struct TextureSetDecompressionTaskData
				{
					FTextureSource* Source;
					FSharedImage* RawImage;
					FString AssetPath;

					int32 BlockIndex = 0;
					int32 LayerIndex = 0;
					int32 MipIndex = 0;
				};
				TArray<TextureSetDecompressionTaskData> DecompressionTasks;
				TMap<UTexture*, TRefCountPtr<FSharedImage>> ProcessedSourceTextures;

				for (TMap<FName, TObjectPtr<class UTexture>>::TConstIterator It(SourceTextures); It; ++It)
				{
					if (It.Value())
					{
						UTexture* SrcTexture = It.Value().Get();

						// Handle the duplication of source texture, for example, same source texture is used for multiple modules
						TRefCountPtr<FSharedImage>* ProcessedSourceeTexture = ProcessedSourceTextures.Find(SrcTexture);
						if (ProcessedSourceeTexture == nullptr)
						{
							if (NumSlices == -1)
							{
								SizeX = SrcTexture->Source.GetSizeX();
								SizeY = SrcTexture->Source.GetSizeY();
								NumSlices = SrcTexture->Source.GetNumSlices();
							}
							else if ((SizeX != SrcTexture->Source.GetSizeX()) || (SizeY != SrcTexture->Source.GetSizeY()) || (NumSlices != SrcTexture->Source.GetNumSlices()))
							{
								UE_LOG(LogTemp, Error, TEXT("Invalid Source Texture Resolution (%s)(%s) within TextureSet."), *(It.Key().ToString()), *(It.Value().GetPathName()));
								continue;
							}


							TRefCountPtr<FSharedImage> ImagePtr = new FSharedImage(0, 0, 0, ERawImageFormat::BGRA8, EGammaSpace::Linear);

							SourceRawImages.Add(It.Key(), ImagePtr);
							ProcessedSourceTextures.Add(SrcTexture, ImagePtr);

							TextureSetDecompressionTaskData Task;
							Task.Source = &SrcTexture->Source;
							Task.RawImage = ImagePtr;
							Task.AssetPath = It.Value().GetPathName();
							DecompressionTasks.Add(Task);

						}
						else
						{
							SourceRawImages.Add(It.Key(), *ProcessedSourceeTexture);
						}
					}
				}

				// Parallel decompression
				{
					int32 NumJobs = DecompressionTasks.Num();

					ParallelFor(TEXT("TextureSet.DecompressSourceImage.PF"), NumJobs, 1, [&](int32 Index)
						{
							TextureSetDecompressionTaskData& Task = DecompressionTasks[Index];
							if (!Task.Source->GetMipImage(*Task.RawImage, Task.BlockIndex, Task.LayerIndex, Task.MipIndex))
							{
								checkf(false, TEXT("Fail to decompress source texture (%s) within TextureSet."), *Task.AssetPath);
							}
						});
				}
			}

			// Texture Set Module should process the source images and generate the processed images here
			{
				// Add the procedurally-generated image data into SourceRawImages
			}

			IsTextureSetProcessed = true;
			CookedTexturesProcessedBitmask = 0;
		}
	}

	// Packing
	{
		// this section is running in parallel, should only read the images data from SourceRawImages

		bool IsHDRTexture = UE::TextureDefines::IsHDR(TextureSetPackedTextureDef.CompressionSettings);
		bool IsSRGB = TextureSetPackedTextureDef.GetHardwareSRGBEnabled();
		int32 ChannelCount = TextureSetPackedTextureDef.UsedChannels();

		ERawImageFormat::Type ImageFormat = ERawImageFormat::Invalid;
		if (IsHDRTexture)
		{
			if (ChannelCount == 1)
				ImageFormat = ERawImageFormat::R32F;
			else
				ImageFormat = ERawImageFormat::RGBA32F;
		}
		else
		{
			if (ChannelCount == 1)
				ImageFormat = ERawImageFormat::G8;
			else
				ImageFormat = ERawImageFormat::BGRA8;
		}
		check(ImageFormat != ERawImageFormat::Invalid);
		EGammaSpace GammaSpace = IsSRGB ? EGammaSpace::sRGB : EGammaSpace::Linear;

		const TArray<FString>& SourcesWithoutChannel = TextureSetPackedTextureDef.GetSourcesWithoutChannel(false);
		TRefCountPtr<FSharedImage> SourceImages[4] = {nullptr, nullptr, nullptr, nullptr};
		FSharedImage* AnySourceImage = nullptr;
		for (int32 ChannelIdx = 0; ChannelIdx < ChannelCount; ChannelIdx++)
		{
			const FString& Source = SourcesWithoutChannel[ChannelIdx];
			TRefCountPtr<FSharedImage>* SourceChannelImage = SourceRawImages.Find(FName(*Source));
			if ((SourceChannelImage != nullptr) && (*SourceChannelImage))
			{
				SourceImages[ChannelIdx] = *SourceChannelImage;
				AnySourceImage = *SourceChannelImage;
			}
		}

		uint8 DefaultValue[16] = { 0 };
		FVector4 DefaultColor = PackingInfo.GetPackedTextureInfo(PackedTextureDefIndex).DefaultValue;
		if (IsHDRTexture)
		{		
			float* DefaultColorValue = (float*)DefaultValue;
			DefaultColorValue[0] = DefaultColor[0];
			DefaultColorValue[1] = DefaultColor[1];
			DefaultColorValue[2] = DefaultColor[2];
			DefaultColorValue[3] = DefaultColor[3];
		}
		else
		{
			uint8* DefaultColorValue = DefaultValue;
			DefaultColorValue[0] = DefaultColor[0] * 255.0f;
			DefaultColorValue[1] = DefaultColor[1] * 255.0f;
			DefaultColorValue[2] = DefaultColor[2] * 255.0f;
			DefaultColorValue[3] = DefaultColor[3] * 255.0f;
		}

		int32 SizeX = AnySourceImage ? AnySourceImage->SizeX : 4;
		int32 SizeY = AnySourceImage ? AnySourceImage->SizeY : 4;
		int32 NumSlices = AnySourceImage ? AnySourceImage->NumSlices : 1;

		TRefCountPtr<FSharedImage> PackedImagePtr = new FSharedImage(SizeX, SizeY, NumSlices, ImageFormat, GammaSpace);

		TRefCountPtr<FSharedImage> ConvertedSourceImages[4] = { nullptr, nullptr, nullptr, nullptr };
		for (int32 ChannelIdx = 0; ChannelIdx < ChannelCount; ChannelIdx++)
		{
			if (SourceImages[ChannelIdx] != nullptr)
			{
				FSharedImage* ConvertedImage = nullptr;
				for (int32 PrevChannelIdx = 0; PrevChannelIdx < ChannelIdx; PrevChannelIdx++)
				{
					if (SourceImages[PrevChannelIdx].GetReference() == SourceImages[ChannelIdx].GetReference())
						ConvertedImage = ConvertedSourceImages[PrevChannelIdx];
				}

				// source image has been NOT converted to target format and gamma space, so convert the image
				if (ConvertedImage == nullptr)
				{
					// if input and output are in same format and gamma space, just pass the shared image through; otherwise, copy and convert the image
					if ((SourceImages[ChannelIdx]->Format == ImageFormat) && (SourceImages[ChannelIdx]->GammaSpace == GammaSpace))
					{
						ConvertedImage = SourceImages[ChannelIdx];
					}
					else
					{
						ConvertedImage = new FSharedImage(SizeX, SizeY, NumSlices, ImageFormat, GammaSpace);
						SourceImages[ChannelIdx]->CopyTo(*ConvertedImage, ImageFormat, GammaSpace);
					}
				}

				ConvertedSourceImages[ChannelIdx] = ConvertedImage;
			}
		}

		{
			int32 NumRowsEachJob;
			int32 NumJobs = ImageParallelForComputeNumJobsForRows(NumRowsEachJob, PackedImagePtr->SizeX, PackedImagePtr->SizeY);

			for (int32 Slice = 0; Slice < NumSlices; Slice++)
			{
				uint8* DstSlice = (uint8*)PackedImagePtr->GetSlice(Slice).RawData;
				uint8* SrcSlice[4] = { nullptr };
				for (int32 ChannelIdx = 0; ChannelIdx < ChannelCount; ChannelIdx++)
				{
					if (ConvertedSourceImages[ChannelIdx] != nullptr)
						SrcSlice[ChannelIdx] = (uint8*)ConvertedSourceImages[ChannelIdx]->GetSlice(Slice).RawData;
				}

				int32 BytesPerChannel = GetBytesPerChannel(ImageFormat);
				int32 BytesPerPixel = PackedImagePtr->GetBytesPerPixel();
				int32 BytesPerRow = BytesPerPixel * PackedImagePtr->SizeX;
				int32 SliceSizeBytes = PackedImagePtr->GetSliceSizeBytes();

				// fill the default color
				int32 NumPixels = PackedImagePtr->SizeX * PackedImagePtr->SizeY;
				uint8* DstSliceStartAddr = DstSlice;
				for (int32 p = 0; p < NumPixels; p++)
				{
					FMemory::Memcpy(DstSliceStartAddr, DefaultValue, BytesPerPixel);
					DstSliceStartAddr += BytesPerPixel;
				}

				// If there is actual source image data, then fill into the packed texture, otherwise use the default color filled above
				if (AnySourceImage)
				{

					ParallelFor(TEXT("TextureSet.PackingTexture.PF"), NumJobs, 1, [&](int32 Index)
						{
							int32 StartIndex = Index * NumRowsEachJob;
							int32 EndIndex = FMath::Min(StartIndex + NumRowsEachJob, PackedImagePtr->SizeY);
							for (int32 DestY = StartIndex; DestY < EndIndex; ++DestY)
							{
								uint8* DstRowBaseAddress = DstSlice + BytesPerRow * DestY;
								uint8* SrcRowBaseAddress[4];
								for (int32 ChannelIdx = 0; ChannelIdx < ChannelCount; ChannelIdx++)
								{
									SrcRowBaseAddress[ChannelIdx] = SrcSlice[ChannelIdx] + BytesPerRow * DestY;
								}

								for (int32 DestX = 0; DestX < PackedImagePtr->SizeX; DestX++)
								{
									uint8* DstBaseAddress = DstRowBaseAddress + BytesPerPixel * DestX;
									for (int32 ChannelIdx = 0; ChannelIdx < ChannelCount; ChannelIdx++)
									{
										if (SrcSlice[ChannelIdx] != nullptr)
										{
											uint8* SrcBaseAddress = SrcRowBaseAddress[ChannelIdx] + BytesPerPixel * DestX;
											FMemory::Memcpy(DstBaseAddress + ChannelIdx * BytesPerChannel, SrcBaseAddress + ChannelIdx * BytesPerChannel, BytesPerChannel);
										}
									}
								}
							}
						});

				}

			}
		}

		// Copy packed image data back to the cooked texture
		{
			CookedTexture->Source.Init((FImageView)*PackedImagePtr);

			// update Source Guid
			//const FString& DDCKey = PackedTextureKeys[PackedTextureDefIndex];
			//const uint32 KeyLength = DDCKey.Len() * sizeof(DDCKey[0]);
			//uint32 Hash[5];
			//FSHA1::HashBuffer(*DDCKey, KeyLength, reinterpret_cast<uint8*>(Hash));
			//FGuid SourceGuid = FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);
			const FGuid SourceGuid(0xA7820CFB, 0x20A74359, 0x8C542C14, 0x9623CF50);
			CookedTexture->Source.SetId(SourceGuid, false);
		}
	}

	// Free temporary image raw data if all cooked textures have been cached
	if (CVarTextureSetFreeImmediateImages.GetValueOnAnyThread())
	{
		FScopeLock Lock(&TextureSetCS);

		CookedTexturesProcessedBitmask |= 1 << PackedTextureDefIndex;

		int32 CookedTexturesMask = (1 << CookedTextures.Num()) - 1;
		if (CookedTexturesProcessedBitmask == CookedTexturesMask)
		{
			SourceRawImages.Empty();
			IsTextureSetProcessed = false;
		}
	}
}