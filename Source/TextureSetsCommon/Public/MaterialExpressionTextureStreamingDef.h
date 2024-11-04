// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "MaterialExpressionTextureStreamingDef.generated.h"

UCLASS(collapsecategories)
class TEXTURESETSCOMMON_API UMaterialExpressionTextureStreamingDef : public UMaterialExpressionTextureSample
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
};
