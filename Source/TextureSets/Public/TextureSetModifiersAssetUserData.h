// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "Engine/TextureModifiersAssetUserData.h"
#include "TextureSetModifiersAssetUserData.generated.h"

class UTextureSet;
class UTextureSetDefinition;

UCLASS()
class TEXTURESETS_API UTextureSetModifiersAssetUserData : public UTextureModifiersAssetUserData
{
	GENERATED_BODY()

public:
	/** Begin UAssetUserData overrides */
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;	
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PostEditChangeOwner() override;
#endif // WITH_EDITOR
	/** End UAssetUserData overrides */

	/** Returns the unique ID string for this source art. */
	virtual FString GetIdString() const override;

	virtual void ModifyTextureSource(UTexture* TextureAsset) override;

	UPROPERTY()
	TObjectPtr<UTextureSet> TextureSet;

	UPROPERTY()
	int PackedTextureDefIndex;
};

