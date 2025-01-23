// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "TextureSetThumbnailRenderer.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "NormalMapPreview.h"
#include "Texture2DPreview.h"
#include "TextureResource.h"
#include "UObject/ConstructorHelpers.h"
#include "TextureSet.h"
#include "ThumbnailRendering/ThumbnailManager.h"

UTextureSetThumbnailRenderer::UTextureSetThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UTextureSetThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	return IsValid(Cast<UTextureSet>(Object));
}

void UTextureSetThumbnailRenderer::GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	OutWidth = 0;
	OutHeight = 0;

	// Use the largest source texture to determine our thumbnail size
	UTextureSet* TextureSet = Cast<UTextureSet>(Object);
	if (TextureSet != nullptr)
	{
		UTexture* LargestTexture = nullptr;

		for (auto& [Name, TextureRef] : TextureSet->SourceTextures)
		{
			if (!IsValid(LargestTexture) || (TextureRef.Valid() && TextureRef.GetTexture()->GetSurfaceWidth() > LargestTexture->GetSurfaceWidth()))
			{
				LargestTexture = TextureRef.GetTexture();
			}
		}

		Super::GetThumbnailSize(LargestTexture, Zoom, OutWidth, OutHeight);
	}
}

void UTextureSetThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	// We use the source textures to draw the thumbnail, as they are clearer to the user and we don't need to compile the texture set to do so.

	UTextureSet* TextureSet = Cast<UTextureSet>(Object);
	if (TextureSet != nullptr)
	{
		TArray<UTexture*> PreviewTextures;
		TArray<int32> ChannelMasks;

		for (auto& [Name, TextureRef] : TextureSet->SourceTextures)
		{
			if (TextureRef.Valid())
			{
				PreviewTextures.Add(TextureRef.GetTexture());
				ChannelMasks.Add(TextureRef.ChannelMask);
			}
		}

		const float ItemWidth = (float)Width / (float)PreviewTextures.Num();

		FVector2D UV0(0, 0);
		FVector2D UV1(ItemWidth / Width, 1.0f);
		FVector2D Size(ItemWidth, Height);
		FVector2D Position(X, Y);

		for (int i = 0; i < PreviewTextures.Num(); i++, Position.X += ItemWidth)
		{
			UTexture* PreviewTexture = PreviewTextures[i];

			// Use A canvas tile item to draw
			if (PreviewTexture->GetResource() == nullptr)
			{
				continue;
			}

			FCanvasTileItem CanvasTile(Position, PreviewTexture->GetResource(), Size, UV0, UV1, FLinearColor::White);
			
			if (UTexture2DArray* Texture2DArray = Cast<UTexture2DArray>(PreviewTexture)) 
			{
				bool bIsNormalMap = Texture2DArray->IsNormalMap();
				bool bIsSingleChannel = true;
				CanvasTile.BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters((float)0, (float)0, (float)-1, bIsNormalMap, bIsSingleChannel, false, false, true, false);
			}
			else if (PreviewTexture->IsNormalMap())
			{
				CanvasTile.BatchedElementParameters = new FNormalMapBatchedElementParameters();
			}
		
			int32 ChannelMask = ChannelMasks[i];

			// If only one channel is active, then desaturate the preview
			int ActiveChannels = 0;
			for (int c = 0; c < 3; c++)
			{
				if (ChannelMask & (1 << c))
					ActiveChannels++;
			}

			if (ActiveChannels == 1)
				ChannelMask |= (1 << 4); // Desaturate

			CanvasTile.BlendMode = (ESimpleElementBlendMode)(SE_BLEND_Opaque | (SE_BLEND_RGBA_MASK_START + ChannelMask));
			CanvasTile.Draw(Canvas);
		}

		// Draw "TS" text overlay
		{
			auto TSChars = TEXT("TS");
			int32 TSWidth = 0;
			int32 TSHeight = 0;
			UFont* Font = GEngine->GetLargeFont();
			StringSize(Font, TSWidth, TSHeight, TSChars);
			float PaddingX = Width / 128.0f;
			float PaddingY = Height / 128.0f;
			float ScaleX = Width / 64.0f; //Text is 1/64'th of the size of the thumbnails
			float ScaleY = Height / 64.0f;

			FCanvasTextItem TextItem(FVector2D(Width - PaddingX - TSWidth * ScaleX, Height - PaddingY - TSHeight * ScaleY), FText::FromString(TSChars), Font, FLinearColor::White);
			TextItem.EnableShadow(FLinearColor::Black);
			TextItem.Scale = FVector2D(ScaleX, ScaleY);
			TextItem.Draw(Canvas);
		}
	}
}
