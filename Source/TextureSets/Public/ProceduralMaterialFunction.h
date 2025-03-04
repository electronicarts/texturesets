// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "ProceduralMaterialFunction.generated.h"

UCLASS(Abstract, HideCategories=(MaterialExpressionMaterialFunctionCall))
class TEXTURESETS_API UProceduralMaterialFunction : public UMaterialExpressionMaterialFunctionCall
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UObject Interface.
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface.

	//~ Begin UMaterialExpression Interface.
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface.

	//~ Begin UMaterialExpressionMaterialFunctionCall Interface.
#if WITH_EDITOR
	virtual bool SetMaterialFunction(UMaterialFunctionInterface* NewMaterialFunction) override;
#endif
	//~ End UMaterialExpressionMaterialFunctionCall Interface.

protected:
#if WITH_EDITOR
	bool UpdateMaterialFunction();
#endif

	virtual uint32 ComputeMaterialFunctionHash() { unimplemented(); return 0; }
	virtual bool ConfigureMaterialFunction(UMaterialFunction* NewMaterialFunction) { unimplemented(); return false; }
};
