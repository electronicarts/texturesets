// Copyright (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetModule.h"
#include "ProceduralMappingModule.generated.h"

UCLASS()
class TEXTURESETS_API UProceduralMappingModule : public UTextureSetModule
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UTextureSetSampleParams> GetSampleParamClass() const override;

#if WITH_EDITOR
	virtual int32 ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const override;
	virtual void ConfigureSamplingGraphBuilder(const UMaterialExpressionTextureSetSampleParameter* SampleExpression,
		FTextureSetMaterialGraphBuilder* Builder) const override;
#endif
};
