// (c) Electronic Arts. All Rights Reserved.

#include "BakerModule.h"

#include "TextureSetProcessingGraph.h"
#include "TextureSetMaterialGraphBuilder.h"
#include "TextureSetDefinition.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "BakeUtil.h"
#include "StaticMeshCompiler.h"
#include "MeshDescription.h"
#include "ProcessingNodes/IProcessingNode.h"

void UBakerAssetParams::FixupData(UObject* Outer)
{
	// Update baker params to match the baker modules in the definition

	UTextureSet* TextureSet = CastChecked<UTextureSet>(Outer);
	UTextureSetDefinition* Definition = TextureSet->Definition;
	
	if (IsValid(Definition))
	{
		TMap<FName, FBakerInstanceParam> OldBakerParams = BakerParams;
		BakerParams.Empty(OldBakerParams.Num());

		for (const UTextureSetModule* Module : Definition->GetModuleInfo().GetModules())
		{
			const UBakerModule* BakerModule = Cast<UBakerModule>(Module);
			if (IsValid(BakerModule))
			{
				if (OldBakerParams.Contains(BakerModule->ElementName))
				{
					BakerParams.Add(BakerModule->ElementName, OldBakerParams.FindChecked(BakerModule->ElementName));
				}
				else
				{
					BakerParams.Add(BakerModule->ElementName, FBakerInstanceParam());
				}
			}

		}
	}
}

class FTextureBaker : public ITextureProcessingNode
{
public:
	FTextureBaker(FName Name) : ITextureProcessingNode()
		, Name(Name)
		, BakeArgs()
		, BakeResults()
	{
	}

	virtual FName GetNodeTypeName() const  { return "TextureBake"; }

	virtual const FTextureSetProcessedTextureDef GetTextureDef() override
	{
		FTextureSetProcessedTextureDef Def;
		Def.ChannelCount = 1;
		Def.Encoding = (int)ETextureSetChannelEncoding::RangeCompression;
		Def.Flags = (int)ETextureSetTextureFlags::Default;
		return Def;
	}

	virtual void LoadResources(const FTextureSetProcessingContext& Context) override
	{
		const UBakerAssetParams* BakerModuleAssetParams = Context.AssetParams.Get<UBakerAssetParams>();
		const FBakerInstanceParam* BakerParams = BakerModuleAssetParams->BakerParams.Find(Name);
		if (BakerParams)
		{
			BakeArgs.BakeWidth = BakerParams->BakedTextureWidth;
			BakeArgs.BakeHeight = BakerParams->BakedTextureHeight;

			if (IsValid(BakerParams->SourceMesh))
			{
				if (BakerParams->SourceMesh->IsCompiling())
				{
					FStaticMeshCompilingManager::Get().FinishCompilation({ BakerParams->SourceMesh });
				}

				BakeArgs.SourceModel = &BakerParams->SourceMesh->GetSourceModel(0);
			}
		}
	}

	virtual void Initialize(const FTextureSetProcessingGraph& Graph) override
	{
		// TODO: Cache the result in the DDC
		if (BakeArgs.SourceModel)
		{
			BakeResults.Pixels.SetNumUninitialized(BakeArgs.BakeWidth * BakeArgs.BakeHeight);
			BakeUtil::BakeUV(BakeArgs, BakeResults);
		}
	}

	virtual const uint32 ComputeGraphHash() const override
	{
		return HashCombine(GetTypeHash(Name.ToString()), GetTypeHash(GetNodeTypeName().ToString()));
	}

	virtual const uint32 ComputeDataHash(const FTextureSetProcessingContext& Context) const override
	{
		uint32 Hash = 0;
		Hash = HashCombine(Hash, GetTypeHash(BakeArgs.BakeWidth));
		Hash = HashCombine(Hash, GetTypeHash(BakeArgs.BakeHeight));

		if (BakeArgs.SourceModel != nullptr && IsValid(BakeArgs.SourceModel->StaticMeshDescriptionBulkData))
		{
			const FString SourceID = BakeArgs.SourceModel->StaticMeshDescriptionBulkData->GetBulkData().GetIdString();
			Hash = HashCombine(Hash, GetTypeHash(SourceID));
		}
		
		return Hash;
	}

	virtual FTextureDimension GetTextureDimension() const override { return { BakeArgs.BakeWidth, BakeArgs.BakeHeight, 1}; }

	void ComputeChunk(const FTextureProcessingChunk& Chunk, float* TextureData) const override
	{
		if (BakeArgs.SourceModel)
		{
			int BakedIndex = 0;
			int ChunkIndex = Chunk.DataStart;
			FIntVector Coord;
			for (Coord.Z = 0; Coord.Z < Chunk.TextureSlices; Coord.Z++)
			{
				for (Coord.Y = 0; Coord.Y < Chunk.TextureHeight; Coord.Y++)
				{
					for (Coord.X = 0; Coord.X < Chunk.TextureWidth; Coord.X++)
					{
						// Fill with baked data
						TextureData[ChunkIndex] = BakeResults.Pixels[BakedIndex];
						BakedIndex++;
						ChunkIndex += Chunk.DataPixelStride;
					}
				}
			}
		}
		else
		{
			int ChunkIndex = Chunk.DataStart;
			FIntVector Coord;
			for (Coord.Z = 0; Coord.Z < Chunk.TextureSlices; Coord.Z++)
			{
				for (Coord.Y = 0; Coord.Y < Chunk.TextureHeight; Coord.Y++)
				{
					for (Coord.X = 0; Coord.X < Chunk.TextureWidth; Coord.X++)
					{
						// Fill with default value
						TextureData[ChunkIndex] = 0.5f;
						ChunkIndex += Chunk.DataPixelStride;
					}
				}
			}
		}
	}

private:
	FName Name;
	BakeUtil::BakeArgs BakeArgs;
	BakeUtil::BakeResults BakeResults;
};

void UBakerModule::ConfigureProcessingGraph(FTextureSetProcessingGraph& Graph) const
{
	Graph.AddOutputTexture(ElementName, MakeShared<FTextureBaker>(ElementName));
}

int32 UBakerModule::ComputeSamplingHash(const FTextureSetAssetParamsCollection* SampleParams) const
{
	uint32 Hash = Super::ComputeSamplingHash(SampleParams);

	Hash = HashCombine(Hash, GetTypeHash(ElementName.ToString()));

	return Hash;
}

void UBakerModule::ConfigureSamplingGraphBuilder(const FTextureSetAssetParamsCollection* SampleParams,
	FTextureSetMaterialGraphBuilder* Builder) const
{
	Builder->AddSampleBuilder(SampleBuilderFunction([this, Builder](FTextureSetSubsampleContext& SampleContext)
	{
		// Simply create a sample result for the element
		SampleContext.AddResult(ElementName, SampleContext.GetProcessedTextureSample(ElementName));
	}));
}
