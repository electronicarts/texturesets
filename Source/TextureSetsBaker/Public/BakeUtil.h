// (c) Electronic Arts. All Rights Reserved.

#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogTextureSetBake, Log, All);

class UStaticMesh;

class BakeUtil
{
public:
	struct BakeArgs
	{
		FStaticMeshSourceModel* SourceModel = nullptr;
		int BakeWidth = 0;
		int BakeHeight = 0;
	};

	struct BakeResults
	{
		TArray<float> Pixels;
	};

	static void Bake(const BakeArgs& Args, BakeResults& Results);
	static void BakeUV(const BakeArgs& Args, BakeResults& Results);
	static void DilateUVs(const BakeArgs& Args, BakeResults& Results, int Iterations);
	static void ReplaceNANValues(BakeResults& Results, float NewValue);

	static inline FIntVector2 IndexToCoordinate(int Index, const BakeArgs& Args)
	{
		const int ClampedIndex = FMath::Clamp(Index, 0, Args.BakeWidth * Args.BakeHeight - 1);
		return FIntVector2(ClampedIndex % Args.BakeWidth, ClampedIndex / Args.BakeWidth);
	}

	static inline int CoordinateToIndex(FIntVector2 Coordinate, const BakeArgs& Args)
	{
		FIntVector2 ClampedCoord = Coordinate;

		ClampedCoord.X = FMath::Clamp(Coordinate.X, 0, Args.BakeWidth - 1);
		ClampedCoord.Y = FMath::Clamp(Coordinate.Y, 0, Args.BakeHeight - 1);

		return ClampedCoord.X + (ClampedCoord.Y * Args.BakeWidth);
	}
};