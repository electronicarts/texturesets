// (c) Electronic Arts. All Rights Reserved.

#include "Modules/FlipbookModule.h"

#include "MaterialExpressionTextureSetSampleParameter.h"
#include "MaterialGraphBuilder/HLSLFunctionCallNodeBuilder.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Misc/DataValidation.h"
#include "ProcessingNodes/ParameterPassthrough.h"
#include "ProcessingNodes/TextureInput.h"
#include "ProcessingNodes/TextureOperator.h"
#include "TextureSetDefinition.h"

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "TextureSets"

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
		Def.Flags |= (uint8)ETextureSetTextureFlags::Array;
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

		FScopeLock Lock(&InitializeCS);

		SubImageWidth = SourceImage->GetWidth() / FramesX;
		SubImageHeight = SourceImage->GetHeight() / FramesY;
		FramesPerImage = FramesX * FramesY;
		
		Slices = SourceImage->GetSlices() * FramesPerImage;
	}

	virtual int GetWidth() const override { return SubImageWidth; }
	virtual int GetHeight() const override { return SubImageHeight; }
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
	mutable FCriticalSection InitializeCS;

	int SubImageWidth;
	int SubImageHeight;
	int FramesPerImage;
	int FramesX;
	int FramesY;
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

		Value.X = Parameter->MotionVectorScale.X;
		Value.Y = Parameter->MotionVectorScale.Y;

		Value.Z = Parameter->FlipbookFramerate;
		if (Parameter->bFlipbookLooping)
			Value.Z = -Value.Z;
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

		Value.W = FrameCount;

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
		FTextureSetSourceTextureDef MotionDef = FTextureSetSourceTextureDef(2, ETextureSetChannelEncoding::RangeCompression, FVector4(0.5, 0.5, 0, 0));
		Graph.AddOutputTexture("MotionVector", Graph.AddInputTexture("MotionVector", MotionDef));
	}

	Graph.AddOutputParameter("FlipbookParams", TSharedRef<IParameterProcessingNode>(new FFlipbookParamsNode()));
}

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

	FGraphBuilderOutputAddress Frame0Index = FGraphBuilderOutputAddress(FlipbookFunctionCallExp, 1);
	FGraphBuilderOutputAddress Frame1Index = FGraphBuilderOutputAddress(FlipbookFunctionCallExp, 2);
	FGraphBuilderOutputAddress FrameBlend = FGraphBuilderOutputAddress(FlipbookFunctionCallExp, 3);

	// Create the subsample definitions
	TArray<FSubSampleDefinition> SubsampleDefinitions;
	if (SampleParams->bBlendFrames)
	{
		UMaterialExpressionOneMinus* OneMinusNode = Builder->CreateExpression<UMaterialExpressionOneMinus>();
		Builder->Connect(FrameBlend, OneMinusNode, 0);
		FGraphBuilderOutputAddress OneMinusFrameBlend = FGraphBuilderOutputAddress(OneMinusNode, 0);

		SubsampleDefinitions.Add(FSubSampleDefinition("Flipbook Frame 0", OneMinusFrameBlend));
		SubsampleDefinitions.Add(FSubSampleDefinition("Flipbook Frame 1", FrameBlend));
	}
	else
	{
		UMaterialExpressionConstant* ConstantNode = Builder->CreateExpression<UMaterialExpressionConstant>();
		ConstantNode->R = 1.0f;

		SubsampleDefinitions.Add(FSubSampleDefinition("Flipbook", FGraphBuilderOutputAddress(ConstantNode, 0)));
	}

	// Add the subsamples
	TArray<SubSampleHandle> SubsampleHandles = Builder->AddSubsampleGroup(SubsampleDefinitions);

	Builder->AddSampleBuilder(SampleBuilderFunction([this, Builder, SubsampleDefinitions, SubsampleHandles, Frame0Index, Frame1Index, FrameBlend, FlipbookParams](FTextureSetSubsampleContext& SampleContext)
	{
		const bool bNextFrame = SubsampleHandles.Num() > 1 && SampleContext.IsRelevant(SubsampleHandles[1]);

		const FGraphBuilderOutputAddress& FrameIndex = bNextFrame ? Frame1Index : Frame0Index;

		SampleContext.SetSharedValue(FrameIndex, EGraphBuilderSharedValueType::ArrayIndex);

		if (bUseMotionVectors)
		{
			HLSLFunctionCallNodeBuilder MVFunctionCall("FlipbookMotionVector", "/Plugin/TextureSets/Flipbook.ush");

			const auto [MVIndex0, MVChannel0] = Builder->GetPackingSource("MotionVector.r");
			const auto [MVIndex1, MVChannel1] = Builder->GetPackingSource("MotionVector.g");
			check(MVIndex0 == MVIndex1); // Should be enforced by IsDefinitionValid, but just double-check

			auto [Mul, Add] = Builder->GetRangeCompressParams(MVIndex0);

			FGraphBuilderOutputAddress FrameUVW;
			{
				UMaterialExpression* AppendNode = Builder->CreateExpression<UMaterialExpressionAppendVector>();
				Builder->Connect(SampleContext.GetSharedValue(EGraphBuilderSharedValueType::Texcoord_Raw), AppendNode, 0);
				Builder->Connect(FrameIndex, AppendNode, 1);
				FrameUVW = FGraphBuilderOutputAddress(AppendNode, 0);
			}

			MVFunctionCall.InArgument("FrameUVW", FrameUVW);
			MVFunctionCall.InArgument("MotionVectorTexture", FGraphBuilderOutputAddress(Builder->GetPackedTextureObject(MVIndex0), 0));
			MVFunctionCall.InArgument("MotionVectorSampler", "MotionVectorTextureSampler");
			MVFunctionCall.InArgument("MVChannels", FString::Format(TEXT("int2({0},{1})"), {MVChannel0, MVChannel1}));
			MVFunctionCall.InArgument("MotionVectorMul", Mul);
			MVFunctionCall.InArgument("MotionVectorAdd", Add);
			MVFunctionCall.InArgument("FlipbookParams", FlipbookParams);
			MVFunctionCall.InArgument("FrameTime", FrameBlend, bNextFrame ? " - 1" : "");

			MVFunctionCall.SetReturnType(ECustomMaterialOutputType::CMOT_Float2);

			FGraphBuilderOutputAddress MotionVectorUV(MVFunctionCall.Build(Builder), 0);
			SampleContext.SetSharedValue(MotionVectorUV, EGraphBuilderSharedValueType::Texcoord_Sampling);
		}

	}));
}

EDataValidationResult UFlipbookModule::IsDefinitionValid(const UTextureSetDefinition* Definition, FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (bUseMotionVectors)
	{
		const FTextureSetPackingInfo& Packing = Definition->GetPackingInfo();

		if (Packing.IsPacked("MotionVector.r") && Packing.IsPacked("MotionVector.g"))
		{
			int32 MV_x = Definition->GetPackingInfo().GetPackingSource("MotionVector.r").Key;
			int32 MV_y = Definition->GetPackingInfo().GetPackingSource("MotionVector.g").Key;
			if (MV_x != MV_y)
			{
				Context.AddError(LOCTEXT("MVPacking", "Motion vectors need to be packed into two channels of the same texture"));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	return CombineDataValidationResults(Result, Super::IsDefinitionValid(Definition, Context));
}
#endif

#undef LOCTEXT_NAMESPACE
