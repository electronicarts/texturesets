// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "BakeUtil.h"

#include <embree3/rtcore.h>
#include <embree3/rtcore_ray.h>

#include "Async/ParallelFor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSourceData.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"

DEFINE_LOG_CATEGORY(LogTextureSetBake);

#define LAMBERT_AO 0
#define BENCHMARK_TEXTURESET_BAKE 1

FVector3f GetRayEnd(RTCRay Ray)
{
	FVector3f Origin(Ray.org_x, Ray.org_y, Ray.org_z);
	FVector3f Direction(Ray.dir_x, Ray.dir_y, Ray.dir_z);
	Direction.Normalize(0.01f);
	return Origin + Direction * Ray.tfar;
}

FVector3f CubeToSphere(FVector3f v)
{
	float X = v.X * sqrt(1.0f - v.Y * v.Y * 0.5f - v.Z * v.Z * 0.5f + v.Y * v.Y * v.Z * v.Z / 3.0f);
	float Y = v.Y * sqrt(1.0f - v.Z * v.Z * 0.5f - v.X * v.X * 0.5f + v.Z * v.Z * v.X * v.X / 3.0f);
	float Z = v.Z * sqrt(1.0f - v.X * v.X * 0.5f - v.Y * v.Y * 0.5f + v.X * v.X * v.Y * v.Y / 3.0f);
	return FVector3f(X, Y, Z);
}

FVector3f RandomRayDirection(FVector3f SurfaceNormal)
{
	FVector3f Ray = FVector3f(
		FMath::FRandRange(-1.0f, 1.0f),
		FMath::FRandRange(-1.0f, 1.0f),
		FMath::FRandRange(-1.0f, 1.0f)
	);

	Ray = CubeToSphere(Ray);
	Ray.Normalize();

	float RayDotNormal = FVector3f::DotProduct(Ray, SurfaceNormal);

	if (RayDotNormal < 0.0f)
		Ray = -Ray;

	return Ray;
}

void BakeUtil::Bake(const BakeArgs& Args, BakeResults& Results)
{
	FMeshDescription* MeshDesc = Args.SourceModel->GetOrCacheMeshDescription();

	FStaticMeshConstAttributes Attributes(*MeshDesc);

	TArrayView<const FVector3f> PositionArray = Attributes.GetVertexPositions().GetRawArray();
	TArrayView<const FVertexID> IndexArray = Attributes.GetTriangleVertexIndices().GetRawArray();

	RTCDevice device = rtcNewDevice(NULL);
	check(device);

	RTCScene Scene = rtcNewScene(device);
	check(Scene);
	RTCGeometry Geometry = rtcNewGeometry(device, RTCGeometryType::RTC_GEOMETRY_TYPE_TRIANGLE);
	check(Geometry);

	//rtcSetSharedGeometryBuffer(geometry, RTCBufferType::RTC_BUFFER_TYPE_VERTEX, 0, RTCFormat::RTC_FORMAT_FLOAT3, VertArray.GetData(), 0, sizeof(VertArray[0]), VertArray.Num());
	//rtcSetSharedGeometryBuffer(geometry, RTCBufferType::RTC_BUFFER_TYPE_INDEX, 0, RTCFormat::RTC_FORMAT_UINT3, IndexArray.GetData(), 0, sizeof(IndexArray[0]) * 3, IndexArray.Num() / 3);

	uint32* indices = (uint32*)rtcSetNewGeometryBuffer(Geometry, RTCBufferType::RTC_BUFFER_TYPE_INDEX, 0, RTCFormat::RTC_FORMAT_UINT3, sizeof(uint32) * 3, IndexArray.Num() / 3);
	for (int i = 0; i < IndexArray.Num(); i++)
	{
		indices[i] = (uint32)IndexArray[i].GetValue();
	}

	float* verts = (float*)rtcSetNewGeometryBuffer(Geometry, RTCBufferType::RTC_BUFFER_TYPE_VERTEX, 0, RTCFormat::RTC_FORMAT_FLOAT3, sizeof(float) * 3, PositionArray.Num());
	for (int i = 0; i < PositionArray.Num(); i++)
	{
		verts[i * 3 + 0] = PositionArray[i].X;
		verts[i * 3 + 1] = PositionArray[i].Y;
		verts[i * 3 + 2] = PositionArray[i].Z;
	}

	rtcCommitGeometry(Geometry);
	rtcAttachGeometry(Scene, Geometry);
	rtcReleaseGeometry(Geometry);
	rtcCommitScene(Scene);

	float ViewSize = MeshDesc->GetBounds().BoxExtent.GetAbsMax();

	for (int y = 0; y < Args.BakeHeight; y++)
	{
		for (int x = 0; x < Args.BakeWidth; x++)
		{
			float gradient = (float)x / (float)Args.BakeWidth;

			RTCRayHit rayhit;
			rayhit.ray.org_x = ViewSize;
			rayhit.ray.org_y = -(ViewSize) + ((float)x / (float)Args.BakeWidth) * ViewSize * 2;
			rayhit.ray.org_z = (ViewSize) - ((float)y / (float)Args.BakeHeight) * ViewSize * 2;

			rayhit.ray.dir_x = -1.0f;
			rayhit.ray.dir_y = 0.0f;
			rayhit.ray.dir_z = 0.0f;

			rayhit.ray.tnear = 0.f;
			rayhit.ray.tfar = std::numeric_limits<float>::infinity();

			rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

			RTCIntersectContext context;
			rtcInitIntersectContext(&context);

			rtcIntersect1(Scene, &context, &rayhit);

			float value = 1.0f;

			if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
			{
				value = GetRayEnd(rayhit.ray).X / 100.0f;
			}

			Results.Pixels[(y * Args.BakeWidth) + x] = value;
			
		}
	}

	rtcReleaseScene(Scene);
	rtcReleaseDevice(device);

}

void BakeUtil::BakeUV(const BakeArgs& Args, BakeResults& Results)
{
	#if BENCHMARK_TEXTURESET_BAKE
		UE_LOG(LogTextureSetBake, Log, TEXT("Beginning texture set bake"));
		const double BuildStartTime = FPlatformTime::Seconds();
		double SectionStartTime = BuildStartTime;
	#endif

	FMeshDescription* MeshDesc = Args.SourceModel->GetOrCacheMeshDescription();

	FStaticMeshConstAttributes Attributes(*MeshDesc);

	TArrayView<const FVector3f> PositionArray = Attributes.GetVertexPositions().GetRawArray();
	TArrayView<const FVector3f> NormalArray = Attributes.GetVertexInstanceNormals().GetRawArray();
	TArrayView<const FVertexID> IndexArray = Attributes.GetTriangleVertexIndices().GetRawArray();

	TArrayView<const FVector2f> UVArray = Attributes.GetUVCoordinates(0).GetRawArray();
	TArrayView<const FUVID> UVIndexArray = Attributes.GetTriangleUVIndices().GetRawArray();

	RTCDevice device = rtcNewDevice(NULL);
	check(device);

	// Set up the UV scene
	RTCScene UVscene = rtcNewScene(device);
	check(UVscene);
	RTCGeometry UVGeometry = rtcNewGeometry(device, RTCGeometryType::RTC_GEOMETRY_TYPE_TRIANGLE);
	check(UVGeometry);

	//rtcSetSharedGeometryBuffer(UVGeometry, RTCBufferType::RTC_BUFFER_TYPE_INDEX, 0, RTCFormat::RTC_FORMAT_UINT3, IndexArray.GetData(), 0, sizeof(IndexArray[0]) * 3, IndexArray.Num() / 3);
	uint32* UVIndexBuffer = (uint32*)rtcSetNewGeometryBuffer(UVGeometry, RTCBufferType::RTC_BUFFER_TYPE_INDEX, 0, RTCFormat::RTC_FORMAT_UINT3, sizeof(uint32) * 3, UVIndexArray.Num() / 3);
	for (int i = 0; i < UVIndexArray.Num(); i++)
	{
		UVIndexBuffer[i] = (uint32)UVIndexArray[i].GetValue();
	}


	float* UVVertBuffer = (float*)rtcSetNewGeometryBuffer(UVGeometry, RTCBufferType::RTC_BUFFER_TYPE_VERTEX, 0, RTCFormat::RTC_FORMAT_FLOAT3, sizeof(float) * 3, UVArray.Num());
	for (int i = 0; i < UVArray.Num(); i++)
	{
		UVVertBuffer[i * 3 + 0] = UVArray[i].X;
		UVVertBuffer[i * 3 + 1] = UVArray[i].Y;
		UVVertBuffer[i * 3 + 2] = 0;
	}

	rtcCommitGeometry(UVGeometry);
	rtcAttachGeometry(UVscene, UVGeometry);
	rtcReleaseGeometry(UVGeometry);
	rtcCommitScene(UVscene);

	// Set up the Mesh scene
	RTCScene MeshScene = rtcNewScene(device);
	check(MeshScene);
	RTCGeometry MeshGeometry = rtcNewGeometry(device, RTCGeometryType::RTC_GEOMETRY_TYPE_TRIANGLE);
	check(MeshGeometry);
	
	rtcSetSharedGeometryBuffer(MeshGeometry, RTCBufferType::RTC_BUFFER_TYPE_VERTEX, 0, RTCFormat::RTC_FORMAT_FLOAT3, PositionArray.GetData(), 0, sizeof(PositionArray[0]), PositionArray.Num());
	rtcSetSharedGeometryBuffer(MeshGeometry, RTCBufferType::RTC_BUFFER_TYPE_INDEX, 0, RTCFormat::RTC_FORMAT_UINT3, IndexArray.GetData(), 0, sizeof(IndexArray[0]) * 3, IndexArray.Num() / 3);
	
	rtcCommitGeometry(MeshGeometry);
	rtcAttachGeometry(MeshScene, MeshGeometry);
	rtcReleaseGeometry(MeshGeometry);
	rtcCommitScene(MeshScene);


	struct TraceInput
	{
		int PixelIndex;
		FVector3f Position;
		FVector3f SurfaceNormal;
	};


	ParallelFor(Args.BakeHeight, [&](int32 y)
	{
		TArray<TraceInput> Traces;
		Traces.Empty(Args.BakeWidth);

		for (int x = 0; x < Args.BakeWidth; x++)
		{
			float gradient = (float)x / (float)Args.BakeWidth;

			RTCRayHit rayhit;
			rayhit.ray.org_x = (float)x / (float)Args.BakeWidth;
			rayhit.ray.org_y = (float)y / (float)Args.BakeHeight;
			rayhit.ray.org_z = 1.0f;

			rayhit.ray.dir_x = 0.0f;
			rayhit.ray.dir_y = 0.0f;
			rayhit.ray.dir_z = -1.0f;

			rayhit.ray.tnear = 0.f;
			rayhit.ray.tfar = std::numeric_limits<float>::infinity();

			rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

			RTCIntersectContext context;
			rtcInitIntersectContext(&context);

			rtcIntersect1(UVscene, &context, &rayhit);

			float value = NAN;

			if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
			{
				const uint32 triangle = rayhit.hit.primID;
				const FVector3f positions[3] =
				{
					PositionArray[IndexArray[triangle * 3]],
					PositionArray[IndexArray[triangle * 3 + 1]],
					PositionArray[IndexArray[triangle * 3 + 2]]
				};

				const FVector3f normals[3] =
				{
					NormalArray[IndexArray[triangle * 3]],
					NormalArray[IndexArray[triangle * 3 + 1]],
					NormalArray[IndexArray[triangle * 3 + 2]]
				};

				FVector3f InterpolatedPosition = (positions[1] * rayhit.hit.u) + (positions[2] * rayhit.hit.v) + (positions[0] * (1 - (rayhit.hit.u + rayhit.hit.v)));
				FVector3f InterpolatedNormal = (normals[1] * rayhit.hit.u) + (normals[2] * rayhit.hit.v) + (normals[0] * (1 - (rayhit.hit.u + rayhit.hit.v)));
				InterpolatedNormal.Normalize();
				FVector3f GeometricNormal(FVector3f::CrossProduct(positions[2] - positions[0], positions[1] - positions[0]));
				GeometricNormal.Normalize();

				TraceInput Trace;
				Trace.PixelIndex = (y * Args.BakeWidth) + x;
				Trace.Position = InterpolatedPosition;
				Trace.SurfaceNormal = GeometricNormal;

				Traces.Add(Trace);
			}
			else
			{
				// Fill this pixel with NAN to indicate it will not be tracing a ray
				Results.Pixels[(y * Args.BakeWidth) + x] = NAN;
			}
		}

		// Cast 16 rays per pixel
		RTCIntersectContext Context;
		rtcInitIntersectContext(&Context);

		int32 Valid[16];
		RTCRay16 RayHit;

		int NumSamples = 0;

		for (int t = 0; t < Traces.Num(); t++)
		{
			int Hits = 0;
			int Iterations = 0;
			int MinIterations = 4;
			int MaxIterations = 64;

			float AccumulatedAverage = 0;
			float AccumulatedSqAverage = 0;

			for (int i = 0; i < MaxIterations; i++)
			{
				for (int r = 0; r < 16; r++)
				{
					FVector3f Dir = RandomRayDirection(Traces[t].SurfaceNormal);

					Valid[r] = -1;

					RayHit.org_x[r] = Traces[t].Position.X;
					RayHit.org_y[r] = Traces[t].Position.Y;
					RayHit.org_z[r] = Traces[t].Position.Z;
					RayHit.tnear[r] = 0.1f;


					RayHit.dir_x[r] = Dir.X;
					RayHit.dir_y[r] = Dir.Y;
					RayHit.dir_z[r] = Dir.Z;
					RayHit.time[r] = 0;

					RayHit.tfar[r] = std::numeric_limits<float>::infinity();
				}

				rtcOccluded16(Valid, MeshScene, &Context, &RayHit);

				float SampleAverage = 0;

				for (int r = 0; r < 16; r++)
				{
#if LAMBERT_AO
					float Value = RayHit.tfar[r] == std::numeric_limits<float>::infinity() ? Traces[t].SurfaceNormal.Dot(FVector3f(RayHit.dir_x[r], RayHit.dir_y[r], RayHit.dir_z[r])) : 0;
					Value *= 0.5 * 3.14; // PI for lambertian diffuse, and 0.5 since we only throw rays in a hemisphere aligned to the surface normal
#else
					float Value = RayHit.tfar[r] == std::numeric_limits<float>::infinity() ? 1 : 0;
#endif

					SampleAverage = FMath::Lerp(
							SampleAverage,
							Value,
							1.0f / float(r + 1));
				}

				AccumulatedAverage = FMath::Lerp(AccumulatedAverage, SampleAverage, 1.0f / float(i + 1));
				AccumulatedSqAverage = FMath::Lerp(AccumulatedSqAverage, SampleAverage * SampleAverage, 1.0f / float(i + 1));

				float Variance = FMath::Sqrt(FMath::Abs(AccumulatedSqAverage - (AccumulatedAverage * AccumulatedAverage)));
				float VarianceThreshold = 6.0f / 255.0f;

				NumSamples++;

				if (i > MinIterations && Variance < VarianceThreshold)
				{
					break; // Stop doing iterations when we have converged
				}

			}


			Results.Pixels[Traces[t].PixelIndex] = AccumulatedAverage;
		}

		//UE_LOG(LogTextureSetBake, Log, TEXT("Avarage of %f samples per trace"), (float)NumSamples / (float)Traces.Num());
	});

	rtcReleaseScene(UVscene);
	rtcReleaseScene(MeshScene);
	rtcReleaseDevice(device);

	DilateUVs(Args, Results, 16);
	ReplaceNANValues(Results, 0.0f);

	#if BENCHMARK_TEXTURESET_BAKE
	const double BuildEndTime = FPlatformTime::Seconds();
	UE_LOG(LogTextureSetBake, Log, TEXT("Texture bake took %fs"), BuildEndTime - BuildStartTime);
#endif
}

void BakeUtil::DilateUVs(const BakeArgs& Args, BakeResults& Results, int Iterations)
{
	// TODO: Try a more advanced slope-aware dilation
	// https://shaderbits.com/blog/uv-dilation

	TArray<int> InvalidValues;

	const int32 PixelCount = Results.Pixels.Num();

	InvalidValues.Reserve(PixelCount / 4);

	for (int p = 0; p < PixelCount; p++)
	{
		if (isnan(Results.Pixels[p]))
			InvalidValues.Add(p);
	}

	for (int i = 0; i < Iterations; i++)
	{
		if (InvalidValues.Num() == 0)
			return;

		TArray<TTuple<int, float>> NewValues;
		NewValues.Reserve(InvalidValues.Num() / 2);

		TArray<int> StillInvalidValues;
		StillInvalidValues.Reserve(InvalidValues.Num() / 2);

		for (int p = 0; p < InvalidValues.Num(); p++)
		{
			int InvalidPixel = InvalidValues[p];

			const FIntVector2 InvalidPixelCoord = IndexToCoordinate(InvalidPixel, Args);

			float Neighbors[4]
			{
				Results.Pixels[CoordinateToIndex(InvalidPixelCoord + FIntVector2(1, 0), Args)],
				Results.Pixels[CoordinateToIndex(InvalidPixelCoord + FIntVector2(-1, 0), Args)],
				Results.Pixels[CoordinateToIndex(InvalidPixelCoord + FIntVector2(0, 1), Args)],
				Results.Pixels[CoordinateToIndex(InvalidPixelCoord + FIntVector2(0, -1), Args)],
			};

			float Count = 0;
			float Accumulated = 0.0f;

			for (int n = 0; n < 4; n++)
			{
				if (!isnan(Neighbors[n]))
				{
					Count += 1.0f;
					Accumulated += Neighbors[n];
				}
			}

			if (Count > 0)
			{
				NewValues.Add(TTuple<int, float>(InvalidPixel, Accumulated /= Count));
			}
			else
			{
				StillInvalidValues.Add(InvalidPixel);
			}
		}

		InvalidValues = MoveTemp(StillInvalidValues);

		for (int p = 0; p < NewValues.Num(); p++)
		{
			auto [Index, Value] = NewValues[p];

			Results.Pixels[Index] = Value;
		}
	}
}

void BakeUtil::ReplaceNANValues(BakeResults& Results, float NewValue)
{
	TArray<float>& Pixels = Results.Pixels;
	const int32 PixelCount = Pixels.Num();

	for (int i = 0; i < PixelCount; i++)
	{
		if (isnan(Pixels[i]))
			Pixels[i] = NewValue;
	}
}
