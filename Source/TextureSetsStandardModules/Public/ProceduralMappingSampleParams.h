// Copyright (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetAssetParams.h"
#include "ProceduralMappingSampleParams.generated.h"

UENUM()
enum class ETriplanarMappingMode : uint8
{
	Off UMETA(ToolTip = "Disable triplanar mapping."),
	Triplanar UMETA(ToolTip = "Three texture samples. One sample for each axis."),
	SingleSample UMETA(ToolTip = "Blends the UVs with dithering and samples the texture once."),
};

UCLASS()
class UProceduralMappingSampleParams : public UTextureSetSampleParams
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="ProceduralMapping")
	ETriplanarMappingMode TriplanarMapping;
};