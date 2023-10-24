// (c) Electronic Arts. All Rights Reserved.

#include "Modules/FlipbookModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialGraphBuilder/HLSLFunctionCallNodeBuilder.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "ProcessingNodes/ParameterPassthrough.h"
#include "ProcessingNodes/TextureOperator.h"
#include "ProcessingNodes/TextureInput.h"

#if WITH_EDITOR

class FTextureOperatorSubframe : public FTextureOperator
{
public:
	FTextureOperatorSubframe(TSharedRef<ITextureProcessingNode> I) : FTextureOperator(I)
	{
	}

	virtual FName GetNodeTypeName() const  { return "Subframe"; }

	virtual const uint32 ComputeGraphHash() const override
	{
		uint32 Hash = FTextureOperator::ComputeGraphHash();

		return Hash;
	}

	virtual const FTextureSetProcessedTextureDef GetTextureDef() override
	{
		FTextureSetProcessedTextureDef Def = SourceImage->GetTextureDef();
		Def.Flags |= ETextureSetTextureFlags::Array;
		return Def;
	}

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override
	{
		FTextureOperator::LoadResources(Context);

		const UFlipbookAssetParams* FlipbookAssetParams = Context.GetAssetParam<UFlipbookAssetParams>();
		if (FlipbookAssetParams->FlipbookSourceType == EFlipbookSourceType::TextureSheet)
		{
			FramesX = FMath::Max(1u, FlipbookAssetParams->FlipbookSourceSheetWidth);
			FramesY = FMath::Max(1u, FlipbookAssetParams->FlipbookSourceSheetHeight);
		}
		else
		{
			FramesX = 1;
			FramesY = 1;
		}
	}

	virtual void Initialize(const FTextureSetProcessingGraph& Graph) override
	{
		FTextureOperator::Initialize(Graph);

		SubImageWidth = SourceImage->GetWidth() / FramesX;
		SubImageHeight = SourceImage->GetHeight() / FramesY;
		FramesPerImage = FramesX * FramesY;

		Width = SubImageWidth;
		Height = SubImageHeight;
		Slices = SourceImage->GetSlices() * FramesPerImage;
	}

	virtual int GetWidth() const override { return Width; }
	virtual int GetHeight() const override { return Height; }
	virtual int GetSlices() const override { return Slices; }

	virtual float GetPixel(int X, int Y, int Z, int Channel) const override
	{
		const int SourceZ = Z / FramesPerImage;

		const int SubImageIndex = Z % FramesPerImage;

		const int SourceX = X + (SubImageWidth * (SubImageIndex % FramesX));
		const int SourceY = Y + (SubImageHeight * (SubImageIndex / FramesX));

		return SourceImage->GetPixel(SourceX, SourceY, SourceZ, Channel);
	}

private:
	int SubImageWidth;
	int SubImageHeight;
	int FramesPerImage;
	int FramesX;
	int FramesY;
	int Width;
	int Height;
	int Slices;
};

class FFlipbookParamsNode : public IParameterProcessingNode
{
public:
	FFlipbookParamsNode()
		: Name("FlipbookParamsNode")
		, bInitialized(false)
	{
	}

	virtual FName GetNodeTypeName() const override { return Name; }

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override
	{
		for (const TSharedRef<ITextureProcessingNode>& ProcessingNode : ProcessedTextures)
			ProcessingNode->LoadResources(Context);

		const UFlipbookAssetParams* Parameter = Context.GetAssetParam<UFlipbookAssetParams>();

		Value.Y = Parameter->FlipbookFramerate;
		Value.Z = Parameter->bFlipbookLooping;
		Value.W = Parameter->MotionVectorScale;
	}

	virtual void Initialize(const FTextureSetProcessingGraph& Graph) override
	{
		if (bInitialized)
			return;

		int FrameCount = 0;
		for (auto& [OutputName, ProcessingNode] : Graph.GetOutputTextures())
		{
			ProcessingNode->Initialize(Graph);
			FrameCount = FMath::Max(FrameCount, ProcessingNode->GetSlices());
		}

		Value.X = FrameCount;

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


void UFlipbookModule::ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	// FTextureOperatorSubframe will make all processed textures into texture arrays
	Graph.AddDefaultInputOperator([](TSharedRef<ITextureProcessingNode> Node)
	{
		return TSharedRef<ITextureProcessingNode>(new FTextureOperatorSubframe(Node));
	});

	if (bUseMotionVectors)
	{
		FTextureSetSourceTextureDef MotionDef = FTextureSetSourceTextureDef(2, false, FVector4(0.5, 0.5, 0, 0));
		Graph.AddOutputTexture("MotionVector", Graph.AddInputTexture("MotionVector", MotionDef));
	}

	Graph.AddOutputParameter("FlipbookParams", TSharedRef<IParameterProcessingNode>(new FFlipbookParamsNode()));
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

void UFlipbookModule::ConfigureSamplingGraphBuilder(const UMaterialExpressionTextureSetSampleParameter* SampleExpression, FTextureSetMaterialGraphBuilder* Builder) const
{
	const UFlipbookSampleParams* SampleParams = SampleExpression->SampleParams.Get<UFlipbookSampleParams>();

	UMaterialExpressionFunctionInput* TimeInputExpression = Builder->CreateInput("FlipbookTime", EFunctionInputType::FunctionInput_Scalar);
	TimeInputExpression->PreviewValue = FVector4f(0.0f);
	TimeInputExpression->bUsePreviewValueAsDefault = true;

	HLSLFunctionCallNodeBuilder FlipbookFunctionCall("FlipbookFrame", "/Plugin/TextureSets/Flipbook.ush");

	FGraphBuilderOutputAddress FlipbookParams = FGraphBuilderOutputAddress(Builder->MakeConstantParameter("FlipbookParams", FVector4f(1.0f, 15.0f, 0.0f, 1.0f)), 0);

	FlipbookFunctionCall.InArgument("FlipbookTime", FGraphBuilderOutputAddress(TimeInputExpression, 0));
	FlipbookFunctionCall.InArgument("FlipbookTimeType", FString::FromInt((uint32)SampleParams->FlipbookTimeType));
	FlipbookFunctionCall.InArgument("FlipbookParams", FlipbookParams);

	FlipbookFunctionCall.OutArgument("Frame0", ECustomMaterialOutputType::CMOT_Float1);
	FlipbookFunctionCall.OutArgument("Frame1", ECustomMaterialOutputType::CMOT_Float1);
	FlipbookFunctionCall.OutArgument("FrameBlend", ECustomMaterialOutputType::CMOT_Float1);

	UMaterialExpression* FlipbookFunctionCallExp = FlipbookFunctionCall.Build(Builder);

	FGraphBuilderOutputAddress Frame0 = FGraphBuilderOutputAddress(FlipbookFunctionCallExp, 1);
	FGraphBuilderOutputAddress Frame1 = FGraphBuilderOutputAddress(FlipbookFunctionCallExp, 2);
	FGraphBuilderOutputAddress FrameBlend = FGraphBuilderOutputAddress(FlipbookFunctionCallExp, 3);

	if (SampleParams->bBlendFrames)
	{
		UMaterialExpressionOneMinus* OneMinusNode = Builder->CreateExpression<UMaterialExpressionOneMinus>();
		Builder->Connect(FrameBlend, OneMinusNode, 0);
		FGraphBuilderOutputAddress OneMinusFrameBlend = FGraphBuilderOutputAddress(OneMinusNode, 0);


		const TArray<SubSampleHandle> SubsampleHandles = Builder->AddSubsampleGroup({
			FSubSampleDefinition("Flipbook Frame 0", OneMinusFrameBlend),
			FSubSampleDefinition("Flipbook Frame 1", FrameBlend)
		});

		Builder->AddSampleBuilder(SampleBuilderFunction([this, Builder, SubsampleHandles, Frame0, Frame1](FTextureSetSubsampleContext& SampleContext)
		{
			if (SampleContext.GetAddress().GetHandleChain().Contains(SubsampleHandles[0]))
				SampleContext.SetSharedValue(Frame0, EGraphBuilderSharedValueType::ArrayIndex);
			else if (SampleContext.GetAddress().GetHandleChain().Contains(SubsampleHandles[1]))
				SampleContext.SetSharedValue(Frame1, EGraphBuilderSharedValueType::ArrayIndex);
		}));
	}
	else
	{
		Builder->AddSampleBuilder(SampleBuilderFunction([this, Builder, Frame0](FTextureSetSubsampleContext& SampleContext)
		{
			SampleContext.SetSharedValue(Frame0, EGraphBuilderSharedValueType::ArrayIndex);
		}));
	}
}
#endif
