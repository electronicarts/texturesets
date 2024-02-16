// (c) Electronic Arts. All Rights Reserved.

#include "MaterialExpressionTextureStreamingDef.h"
#include "MaterialCompiler.h"

UMaterialExpressionTextureStreamingDef::UMaterialExpressionTextureStreamingDef(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("Tex")));
}

#if WITH_EDITOR
int32 UMaterialExpressionTextureStreamingDef::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// Magic happens in here; streaming info is generated based on our input Tex and UVs
	Super::Compile(Compiler, OutputIndex);

	// Pass through the texture object to be used in the graph
	return TextureObject.Compile(Compiler);
}
#endif

#if WITH_EDITOR
void UMaterialExpressionTextureStreamingDef::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Texture Streaming Def"));
}
#endif
