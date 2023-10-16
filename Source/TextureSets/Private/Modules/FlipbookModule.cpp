// (c) Electronic Arts. All Rights Reserved.

#include "Modules/FlipbookModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialGraphBuilder/HLSLFunctionCallNodeBuilder.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "ProcessingNodes/ParameterPassthrough.h"

#if WITH_EDITOR

class FFlipbookParamsNode : public IParameterProcessingNode
{
public:
	FFlipbookParamsNode(FTextureSetProcessingGraph& Graph)
		: Name("FlipbookParamsNode")
		, bInitialized(false)
	{
		Graph.GetOutputTextures().GenerateValueArray(ProcessedTextures);
	}

	virtual FName GetNodeTypeName() const override { return Name; }

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override
	{
		for (const TSharedRef<ITextureProcessingNode>& ProcessingNode : ProcessedTextures)
			ProcessingNode->LoadResources(Context);
	}

	virtual void Initialize(const FTextureSetProcessingContext& Context) override
	{
		if (bInitialized)
			return;

		const UFlipbookAssetParams* Parameter = Context.GetAssetParam<UFlipbookAssetParams>();

		int FrameCount = 0;

		for (const TSharedRef<ITextureProcessingNode>& ProcessingNode : ProcessedTextures)
		{
			ProcessingNode->Initialize(Context);
			FrameCount = FMath::Max(FrameCount, ProcessingNode->GetSlices());
		}

		Value = FVector4f(
			FrameCount,
			Parameter->FlipbookFramerate,
			Parameter->bFlipbookLooping,
			Parameter->MotionVectorScale);

		bInitialized = true;
	}

	virtual const uint32 ComputeGraphHash() const override
	{
		uint32 Hash = GetTypeHash(GetNodeTypeName().ToString());

		for (const TSharedRef<ITextureProcessingNode>& ProcessingNode : ProcessedTextures)
			Hash = HashCombine(Hash, ProcessingNode->ComputeGraphHash());

		return Hash;
	}

	virtual const uint32 ComputeDataHash(const FTextureSetProcessingContext& Context) const override
	{
		const UFlipbookAssetParams* Parameter = Context.GetAssetParam<UFlipbookAssetParams>();
		uint32 Hash = 0;
		
		Hash = HashCombine(Hash, GetTypeHash(Parameter->FlipbookFramerate));
		Hash = HashCombine(Hash, GetTypeHash(Parameter->bFlipbookLooping));
		Hash = HashCombine(Hash, GetTypeHash(Parameter->MotionVectorScale));

		for (const TSharedRef<ITextureProcessingNode>& ProcessingNode : ProcessedTextures)
			Hash = HashCombine(Hash, ProcessingNode->ComputeDataHash(Context));

		return Hash;
	}

	virtual FVector4f GetValue() const override
{
		check(bInitialized);
		return Value;
	}

private:
	FName Name;
	TArray<TSharedRef<ITextureProcessingNode>> ProcessedTextures;

	bool bInitialized;

	FVector4f Value;
};


void UFlipbookModule::GenerateProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	// Flag textures as an array
	Graph.SetTextureFlagDefault(ETextureSetTextureFlags::Array);

	if (bUseMotionVectors)
	{
		FTextureSetSourceTextureDef MotionDef = FTextureSetSourceTextureDef(2, false, FVector4(0.5, 0.5, 0, 0));
		Graph.AddOutputTexture("MotionVector", Graph.AddInputTexture("MotionVector", MotionDef));
	}

	Graph.AddOutputParameter("FlipbookParams", TSharedRef<IParameterProcessingNode>(new FFlipbookParamsNode(Graph)));
}
#endif

#if WITH_EDITOR
int32 UFlipbookModule::ComputeSamplingHash(const UMaterialExpressionTextureSetSampleParameter* SampleExpression) const
{
	const UFlipbookSampleParams* HeightSampleParams = SampleExpression->SampleParams.Get<UFlipbookSampleParams>();

	uint32 Hash = Super::ComputeSamplingHash(SampleExpression);

	Hash = HashCombine(Hash, GetTypeHash(HeightSampleParams->bBlendFrames));
	Hash = HashCombine(Hash, GetTypeHash((uint32)HeightSampleParams->FlipbookTimeType));

	Hash = HashCombine(Hash, GetTypeHash(bUseMotionVectors)); // May not need to explicitly hash this as it adds a new texture, but can't hurt

	return Hash;
}

void UFlipbookModule::GenerateSamplingGraph(const UMaterialExpressionTextureSetSampleParameter* SampleExpression, FTextureSetMaterialGraphBuilder& Builder) const
{
	const UFlipbookSampleParams* SampleParams = SampleExpression->SampleParams.Get<UFlipbookSampleParams>();

	UMaterialExpressionFunctionInput* TimeInputExpression = Builder.CreateInput("FlipbookTime", EFunctionInputType::FunctionInput_Scalar);
	TimeInputExpression->PreviewValue = FVector4f(0.0f);
	TimeInputExpression->bUsePreviewValueAsDefault = true;

	HLSLFunctionCallNodeBuilder FlipbookFunctionCall("FlipbookFrame", "/Plugin/TextureSets/Flipbook.ush");

	FGraphBuilderOutputAddress FlipbookParams = FGraphBuilderOutputAddress(Builder.MakeConstantParameter("FlipbookParams", FVector4f(1.0f, 15.0f, 0.0f, 1.0f)), 0);

	FlipbookFunctionCall.InArgument("FlipbookTime", FGraphBuilderOutputAddress(TimeInputExpression, 0));
	FlipbookFunctionCall.InArgument("FlipbookTimeType", FString::FromInt((uint32)SampleParams->FlipbookTimeType));
	FlipbookFunctionCall.InArgument("FlipbookParams", FlipbookParams);

	FlipbookFunctionCall.OutArgument("Frame0", ECustomMaterialOutputType::CMOT_Float1);
	FlipbookFunctionCall.OutArgument("Frame1", ECustomMaterialOutputType::CMOT_Float1);
	FlipbookFunctionCall.OutArgument("FrameBlend", ECustomMaterialOutputType::CMOT_Float1);

	UMaterialExpression* FlipbookFunctionCallExp = FlipbookFunctionCall.Build(Builder);

	if (SampleParams->bBlendFrames)
	{
		// Not yet implemented
	}
	else
	{
		Builder.SetSharedValue(FGraphBuilderOutputAddress(FlipbookFunctionCallExp, 1), EGraphBuilderSharedValueType::ArrayIndex);
	}
}
#endif
