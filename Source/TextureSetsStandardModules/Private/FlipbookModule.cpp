// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "FlipbookModule.h"

#include "TextureSetProcessingGraph.h"
#include "TextureSetMaterialGraphBuilder.h"
#include "TextureSetDefinition.h"
#include "HLSLFunctionCallNodeBuilder.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Misc/DataValidation.h"
#include "ProcessingNodes/TextureOperator.h"
#include "ProcessingNodes/TextureInput.h"

#define LOCTEXT_NAMESPACE "TextureSets"

class FTextureOperatorSubframe : public FTextureOperator
{
public:
	FTextureOperatorSubframe(TSharedRef<ITextureProcessingNode> I) : FTextureOperator(I)
	{
	}

	virtual FName GetNodeTypeName() const  { return "Subframe V1"; }

	virtual const FTextureSetProcessedTextureDef GetTextureDef() const override
	{
		FTextureSetProcessedTextureDef Def = SourceImage->GetTextureDef();
		Def.Flags |= (uint8)ETextureSetTextureFlags::Array;
		return Def;
	}

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override
	{
		FTextureOperator::LoadResources(Context);

		const UFlipbookAssetParams* FlipbookAssetParams = Context.AssetParams.Get<UFlipbookAssetParams>();
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

		FTextureDimension SourceImageDimension = SourceImage->GetTextureDimension();

		SubImageWidth = SourceImageDimension.Width;
		SubImageHeight = SourceImageDimension.Height;
		FramesPerImage = 1;

		if (SourceImageDimension.Width >= FramesX)
		{
			//check(SourceImageDimension.Width % FramesX == 0); // Need to be able to divide the image cleanly into the specified number of frames
			SubImageWidth /= FramesX;
			FramesPerImage *= FramesX;
		}

		if (SourceImageDimension.Height >= FramesY)
		{
			//check(SourceImageDimension.Height % FramesY == 0); // Need to be able to divide the image cleanly into the specified number of frames
			SubImageHeight /= FramesY;
			FramesPerImage *= FramesY;
		}

		Slices = SourceImageDimension.Slices * FramesPerImage;
	}

	virtual FTextureDimension GetTextureDimension() const override { return { SubImageWidth, SubImageHeight, Slices}; }

	FIntVector TransformToSource(FIntVector Position) const
	{
		const int SubImageIndex = Position.Z % FramesPerImage;

		return FIntVector(
			Position.X + (SubImageWidth * (SubImageIndex % FramesX)),
			Position.Y + (SubImageHeight * (SubImageIndex / FramesX)),
			Position.Z / FramesPerImage
		);
	}

	void ComputeChannel(int32 Channel, const FTextureDataTileDesc& Tile, float* TextureData) const override
	{
		if (FramesPerImage == 1)
		{
			// Can early out without remapping the frames
			SourceImage->ComputeChannel(Channel, Tile, TextureData);
			return;
		}
		
		const FTextureDimension SourceImageDimension = SourceImage->GetTextureDimension();
		const FIntVector3 SourceImageSize = FIntVector3(SourceImageDimension.Width, SourceImageDimension.Height, SourceImageDimension.Slices);
		const FTextureSetProcessedTextureDef SourceDef = SourceImage->GetTextureDef();
		
		const FTextureDimension DestDimension = GetTextureDimension();
		const FTextureSetProcessedTextureDef DestDef = GetTextureDef();
		
		const FIntVector3 SubImageSize(SubImageWidth, SubImageHeight, 1);
		
		for (int32 SourceSlice = 0; SourceSlice < SourceImageDimension.Slices; SourceSlice++)
		{
			for (int32 SourceFrameY = 0; SourceFrameY < FramesY; SourceFrameY++)
			{
				for (int32 SourceFrameX = 0; SourceFrameX < FramesX; SourceFrameX++)
				{
					int32 DestSlice = SourceFrameX + (SourceFrameY * FramesX) + (SourceSlice * SourceFrameY * FramesX);

					// Can skip this frame entirely if it's outside the tile we're computing
					if (DestSlice < Tile.TileOffset.Z || DestSlice >= Tile.TileOffset.Z + Tile.TileSize.Z)
						continue;

					FIntVector3 SourceTileSize = FIntVector3(Tile.TileSize.X, Tile.TileSize.Y, 1);
					FIntVector3 SourceTileOffset = FIntVector3(SourceFrameX, SourceFrameY, SourceSlice) * SubImageSize;

					// Shift the source tile offset to account for the requested tile
					SourceTileOffset.X += Tile.TileOffset.X;
					SourceTileOffset.Y += Tile.TileOffset.Y;

					// Create a tile that describes the subimage texture in the texture sheet, but the memory in the texture array
					// This will allow the source image to do the remapping as it reads.
					FTextureDataTileDesc SourceTile(
						SourceImageSize,
						SourceTileSize,
						SourceTileOffset,
						Tile.TileDataStride,
						Tile.TileCoordToDataIndex(FIntVector3(0, 0, DestSlice - Tile.TileOffset.Z))
					);
					
					SourceImage->ComputeChannel(Channel, SourceTile, TextureData);
				}
			}
		}
	}

private:
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

		const UFlipbookAssetParams* Parameter = Context.AssetParams.Get<UFlipbookAssetParams>();

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
			ITextureProcessingNode::FTextureDimension SourceImageDimension = ProcessingNode->GetTextureDimension();

			FrameCount = FMath::Max(FrameCount, SourceImageDimension.Slices);
		}

		Value.W = FrameCount;

		bInitialized = true;
	}

	virtual void ComputeGraphHash(FHashBuilder& HashBuilder) const override
	{
		HashBuilder << GetNodeTypeName().ToString();

		for (const TSharedRef<ITextureProcessingNode>& ProcessingNode : ProcessedTextures)
			ProcessingNode->ComputeGraphHash(HashBuilder);
	}

	virtual void ComputeDataHash(const FTextureSetProcessingContext& Context, FHashBuilder& HashBuilder) const override
	{
		const UFlipbookAssetParams* Parameter = Context.AssetParams.Get<UFlipbookAssetParams>();

		HashBuilder << GetTypeHash(Parameter->FlipbookFramerate);
		HashBuilder << GetTypeHash(Parameter->bFlipbookLooping);
		HashBuilder << GetTypeHash(Parameter->MotionVectorScale);

		for (const TSharedRef<ITextureProcessingNode>& ProcessingNode : ProcessedTextures)
			ProcessingNode->ComputeDataHash(Context, HashBuilder);
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

void UFlipbookModule::GetAssetParamClasses(TSet<TSubclassOf<UTextureSetAssetParams>>& Classes) const
{
	Classes.Add(UFlipbookAssetParams::StaticClass());
}

void UFlipbookModule::GetSampleParamClasses(TSet<TSubclassOf<UTextureSetSampleParams>>& Classes) const
{
	Classes.Add(UFlipbookSampleParams::StaticClass());
}

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

int32 UFlipbookModule::ComputeSamplingHash(const FTextureSetAssetParamsCollection* SampleParams) const
{
	const UFlipbookSampleParams* FlipbookSampleParams = SampleParams->Get<UFlipbookSampleParams>();

	uint32 Hash = Super::ComputeSamplingHash(SampleParams);

	Hash = HashCombine(Hash, GetTypeHash(FlipbookSampleParams->bBlendFrames));
	Hash = HashCombine(Hash, GetTypeHash((uint32)FlipbookSampleParams->FlipbookTimeType));

	Hash = HashCombine(Hash, GetTypeHash(bUseMotionVectors)); // May not need to explicitly hash this as it adds a new texture, but can't hurt

	return Hash;
}

void UFlipbookModule::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams, FTextureSetMaterialGraphBuilder* Builder) const
{
	const UFlipbookSampleParams* FlipbookSampleParams = SampleParams->Get<UFlipbookSampleParams>();

	UMaterialExpressionFunctionInput* TimeInputExpression = Builder->CreateInput("FlipbookTime", EFunctionInputType::FunctionInput_Scalar);
	TimeInputExpression->PreviewValue = FVector4f(0.0f);
	TimeInputExpression->bUsePreviewValueAsDefault = true;

	HLSLFunctionCallNodeBuilder FlipbookFunctionCall("FlipbookFrame", "/Plugin/TextureSets/Flipbook.ush");

	FGraphBuilderOutputAddress FlipbookParams = FGraphBuilderOutputAddress(Builder->MakeConstantParameter("FlipbookParams", FVector4f(1.0f, 15.0f, 0.0f, 1.0f)), 0);

	FlipbookFunctionCall.InArgument("FlipbookTime", FGraphBuilderOutputAddress(TimeInputExpression, 0));
	FlipbookFunctionCall.InArgument("FlipbookTimeType", FString::FromInt((uint32)FlipbookSampleParams->FlipbookTimeType));
	FlipbookFunctionCall.InArgument("FlipbookParams", FlipbookParams);

	FlipbookFunctionCall.OutArgument("Frame0", ECustomMaterialOutputType::CMOT_Float1);
	FlipbookFunctionCall.OutArgument("Frame1", ECustomMaterialOutputType::CMOT_Float1);
	FlipbookFunctionCall.OutArgument("FrameBlend", ECustomMaterialOutputType::CMOT_Float1);

	UMaterialExpression* FlipbookFunctionCallExp = FlipbookFunctionCall.Build(Builder);

	FGraphBuilderOutputAddress Frame0Index = FGraphBuilderOutputAddress(FlipbookFunctionCallExp, 1);
	FGraphBuilderOutputAddress Frame1Index = FGraphBuilderOutputAddress(FlipbookFunctionCallExp, 2);
	FGraphBuilderOutputAddress FrameBlend = FGraphBuilderOutputAddress(FlipbookFunctionCallExp, 3);

	// Create the subsample definitions
	TArray<SubSampleHandle> SubsampleHandles;
	if (FlipbookSampleParams->bBlendFrames)
	{
		UMaterialExpressionOneMinus* OneMinusNode = Builder->CreateExpression<UMaterialExpressionOneMinus>();
		Builder->Connect(FrameBlend, OneMinusNode, 0);
		FGraphBuilderOutputAddress OneMinusFrameBlend = FGraphBuilderOutputAddress(OneMinusNode, 0);

		// Add two subsamples
		SubsampleHandles = Builder->AddSubsampleGroup({
			FSubSampleDefinition("Flipbook Frame 0", OneMinusFrameBlend),
			FSubSampleDefinition("Flipbook Frame 1", FrameBlend)
		});
	}

	Builder->AddSampleBuilder(SampleBuilderFunction([this, Builder, SubsampleHandles, Frame0Index, Frame1Index, FrameBlend, FlipbookParams](FTextureSetSubsampleContext& SampleContext)
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
			MVFunctionCall.InArgument("MotionVectorTexture", Builder->GetPackedTextureObject(MVIndex0, SampleContext.GetSharedValue(EGraphBuilderSharedValueType::Texcoord_Streaming)));
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
			const int32 MVXPackedTexIdx = Definition->GetPackingInfo().GetPackingSource("MotionVector.r").Key;
			const int32 MVYPackedTexIdx = Definition->GetPackingInfo().GetPackingSource("MotionVector.g").Key;

			if (MVXPackedTexIdx != MVYPackedTexIdx)
			{
				Context.AddError(LOCTEXT("MVPacking", "Motion vectors need to be packed into two channels of the same texture"));
				Result = EDataValidationResult::Invalid;
			}

			if (Packing.GetPackedTextureDef(MVXPackedTexIdx).bVirtualTextureStreaming)
			{
				Context.AddError(LOCTEXT("NoVTMotionVectors", "Derived textures containing flipbook motion vectors cannot be virtual textures."));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	return CombineDataValidationResults(Result, Super::IsDefinitionValid(Definition, Context));
}
#undef LOCTEXT_NAMESPACE
