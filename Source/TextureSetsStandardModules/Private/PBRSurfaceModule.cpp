// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "PBRSurfaceModule.h"

#include "TextureSetProcessingGraph.h"
#include "TextureSetSampleFunctionBuilder.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionDeriveNormalZ.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "ProcessingNodes/TextureInput.h"
#include "ProcessingNodes/TextureOperator.h"

class FTextureOperatorFlipNormalGreen : public FTextureOperator
{
public:
	FTextureOperatorFlipNormalGreen(TSharedRef<ITextureProcessingNode> I) : FTextureOperator(I) {}

	virtual FName GetNodeTypeName() const  { return "FlipNormalGreen"; }

	virtual void Prepare(const FTextureSetProcessingContext& Context) override
	{
		FTextureOperator::Prepare(Context);

		const UPBRAssetParams* FlipbookAssetParams = Context.AssetParams.Get<UPBRAssetParams>();

		bFlipGreen = FlipbookAssetParams->bFlipNormalGreen;
	}

	virtual void ComputeDataHash(const FTextureSetProcessingContext& Context, FHashBuilder& HashBuilder) const override
	{ 
		FTextureOperator::ComputeDataHash(Context, HashBuilder);

		const UPBRAssetParams* FlipbookAssetParams = Context.AssetParams.Get<UPBRAssetParams>();
		HashBuilder << (FlipbookAssetParams->bFlipNormalGreen);
	}

	void WriteChannel(int32 Channel, const FTextureDataTileDesc& Tile, float* TextureData) const override
	{
		SourceImage->WriteChannel(Channel, Tile, TextureData);

		if (Channel == 1 && bFlipGreen)
		{
			Tile.ForEachPixel([TextureData](FTextureDataTileDesc::ForEachPixelContext& Context)
			{
				TextureData[Context.DataIndex] = 1.0f - TextureData[Context.DataIndex];
			});
		}
	}

private:
	bool bFlipGreen;
};

void UPBRSurfaceModule::GetAssetParamClasses(TSet<TSubclassOf<UTextureSetAssetParams>>& Classes) const
{
	Classes.Add(UPBRAssetParams::StaticClass());
}

void UPBRSurfaceModule::GetSampleParamClasses(TSet<TSubclassOf<UTextureSetSampleParams>>& Classes) const
{
	Classes.Add(UPBRSampleParams::StaticClass());
}

void UPBRSurfaceModule::ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	if (Paramaterization == EPBRParamaterization::Basecolor_Metal || Paramaterization == EPBRParamaterization::Dielectric)
	{
		// Base Color
		{
			static const FTextureSetSourceTextureDef BaseColorDef(3, ETextureSetChannelEncoding::SRGB, FVector4(0.5, 0.5, 0.5, 0));
			Graph.AddOutputTexture(BaseColorName, Graph.AddInputTexture(BaseColorName, BaseColorDef));
		}

		// Metal
		if (Paramaterization == EPBRParamaterization::Basecolor_Metal)
		{
			static const FTextureSetSourceTextureDef MetalDef(1, ETextureSetChannelEncoding::Default, FVector4(0, 0, 0, 0));
			Graph.AddOutputTexture(MetalName, Graph.AddInputTexture(MetalName, MetalDef));
		}
	}
	else if (Paramaterization == EPBRParamaterization::Albedo_Spec)
	{
		// Albedo
		static const FTextureSetSourceTextureDef AlbedoDef(3, ETextureSetChannelEncoding::SRGB, FVector4(0.5, 0.5, 0.5, 0));
		Graph.AddOutputTexture(AlbedoName, Graph.AddInputTexture(AlbedoName, AlbedoDef));

		// Spec
		static const FTextureSetSourceTextureDef SpecularDef(3, ETextureSetChannelEncoding::SRGB, FVector4(0, 0, 0, 0));
		Graph.AddOutputTexture(SpecularName, Graph.AddInputTexture(SpecularName, SpecularDef));
	}
	else if (Paramaterization != EPBRParamaterization::None)
	{
		unimplemented()
	}

	if (Microsurface == EPBRMicrosurface::Roughness)
	{
		// Roughness
		static const FTextureSetSourceTextureDef RoughnessDef (1, ETextureSetChannelEncoding::RangeCompression, FVector4(0.5, 0.5, 0.5, 0));
		Graph.AddOutputTexture(RoughnessName, Graph.AddInputTexture(RoughnessName, RoughnessDef));
	}
	else if (Microsurface == EPBRMicrosurface::Smoothness)
	{
		// Smoothness
		static const FTextureSetSourceTextureDef SmoothnessDef (1, ETextureSetChannelEncoding::RangeCompression, FVector4(0.5, 0.5, 0.5, 0));
		Graph.AddOutputTexture(SmoothnessName, Graph.AddInputTexture(SmoothnessName, SmoothnessDef));
	}
	else if (Microsurface != EPBRMicrosurface::None)
	{
		unimplemented()
	}

	if (Normal == EPBRNormal::Tangent)
	{
		static const FTextureSetSourceTextureDef TangentNormalDef (2, ETextureSetChannelEncoding::RangeCompression, FVector4(0.5, 0.5, 1, 0));
		TSharedRef<FTextureInput> NormalInput = Graph.AddInputTexture(TangentNormalName, TangentNormalDef);
		
		NormalInput->AddOperator([](TSharedRef<ITextureProcessingNode> Node)
		{
			return TSharedRef<ITextureProcessingNode>(new FTextureOperatorFlipNormalGreen(Node));
		});

		Graph.AddOutputTexture(TangentNormalName,NormalInput);
	}
	else if (Normal != EPBRNormal::None)
	{
		unimplemented();
	}
}

void UPBRSurfaceModule::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
	FTextureSetSampleFunctionBuilder* Builder) const
{
	const UPBRSampleParams* PBRSampleParams = SampleParams->Get<UPBRSampleParams>();

	Builder->AddSubsampleFunction(ConfigureSubsampleFunction([this, Builder, PBRSampleParams](FTextureSetSubsampleBuilder& Subsample)
	{
		// Surface Parameterization
		switch (Paramaterization)
		{
		case EPBRParamaterization::Basecolor_Metal:
			Subsample.AddResult(MetalName, Subsample.GetSharedValue(MetalName));
			// Falls through
		case EPBRParamaterization::Dielectric:
			Subsample.AddResult(BaseColorName, Subsample.GetSharedValue(BaseColorName));
			break;
		case EPBRParamaterization::Albedo_Spec:
			Subsample.AddResult(AlbedoName, Subsample.GetSharedValue(AlbedoName));
			Subsample.AddResult(SpecularName, Subsample.GetSharedValue(SpecularName));
			break;
		default:
			break;
		}

		// Microsurface
		if (Microsurface != EPBRMicrosurface::None && PBRSampleParams->MicrosurfaceOutput != EPBRMicrosurface::None)
		{
			FGraphBuilderOutputPin MicrosurfaceSample;
			if (Microsurface == EPBRMicrosurface::Roughness)
				MicrosurfaceSample = Subsample.GetSharedValue(RoughnessName);
			else if (Microsurface == EPBRMicrosurface::Smoothness)
				MicrosurfaceSample = Subsample.GetSharedValue(SmoothnessName);
			else 
				unimplemented()

			FGraphBuilderOutputPin ResultPin = MicrosurfaceSample;

			if (Microsurface != PBRSampleParams->MicrosurfaceOutput)
			{
				UMaterialExpressionOneMinus* OneMinus = Builder->CreateExpression<UMaterialExpressionOneMinus>();
				Builder->Connect(ResultPin, OneMinus, 0);
				ResultPin = FGraphBuilderOutputPin(OneMinus, 0);
			}

			FName ResultName;

			if (PBRSampleParams->MicrosurfaceOutput == EPBRMicrosurface::Roughness)
				ResultName = RoughnessName;
			else if (PBRSampleParams->MicrosurfaceOutput == EPBRMicrosurface::Smoothness)
				ResultName = SmoothnessName;
			else
				unimplemented()

			Subsample.AddResult(ResultName, ResultPin);
		}

		// Normals
		if (Normal == EPBRNormal::Tangent)
		{
			FGraphBuilderOutputPin TangentNormal = Subsample.GetSharedValue(TangentNormalName);

			// Unpack tangent normal X and Y
			UMaterialExpressionMultiply* Mul = Builder->CreateExpression<UMaterialExpressionMultiply>();
			Builder->Connect(TangentNormal, Mul, 0);
			Mul->ConstB = 2.0f;

			UMaterialExpressionAdd* Add = Builder->CreateExpression<UMaterialExpressionAdd>();
			Builder->Connect(Mul, 0, Add, 0);
			Add->ConstB = -1.0f;

			// Derive Z value
			UMaterialExpressionDeriveNormalZ* DeriveZ = Builder->CreateExpression<UMaterialExpressionDeriveNormalZ>();
			Builder->Connect(Add, 0, DeriveZ, 0);

			if (PBRSampleParams->NormalOutput == EPBRNormalSpace::Tangent)
			{
				// Output tangent normal
				Subsample.AddResult(TangentNormalName, FGraphBuilderOutputPin(DeriveZ, 0));
			}
			else
			{
				UMaterialExpression* Transform3x3 = Builder->CreateFunctionCall(FSoftObjectPath(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Math/Transform3x3Matrix.Transform3x3Matrix")));
				Builder->Connect(DeriveZ, 0, Transform3x3, "VectorToTransform (V3)");
				Builder->Connect(Subsample.GetSharedValue(EGraphBuilderSharedValueType::Tangent), Transform3x3, "BasisX (V3)");
				Builder->Connect(Subsample.GetSharedValue(EGraphBuilderSharedValueType::Bitangent), Transform3x3, "BasisY (V3)");
				Builder->Connect(Subsample.GetSharedValue(EGraphBuilderSharedValueType::BaseNormal), Transform3x3, "BasisZ (V3)");

				if (PBRSampleParams->NormalOutput == EPBRNormalSpace::World)
				{
					// Output world normal
					Subsample.AddResult(WorldNormalName, FGraphBuilderOutputPin(Transform3x3, 0));
				}
				else if (PBRSampleParams->NormalOutput == EPBRNormalSpace::SurfaceGradient)
				{
					UMaterialExpressionSubtract* Subtract = Builder->CreateExpression<UMaterialExpressionSubtract>();
					Builder->Connect(Transform3x3, 0, Subtract, 0);
					Builder->Connect(Subsample.GetSharedValue(EGraphBuilderSharedValueType::BaseNormal), Subtract, 1);

					// Output surface gradient
					Subsample.AddResult(SurfaceGradientName, FGraphBuilderOutputPin(Subtract, 0));
				}
				else
				{
					unimplemented();
				}
			}
		}
	}));
}
