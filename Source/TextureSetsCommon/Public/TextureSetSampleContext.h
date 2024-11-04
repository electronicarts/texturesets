// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

UENUM()
enum class EBaseNormalSource : uint8
{
	Explicit UMETA(ToolTip = "Explicitly define base normal input."),
	Vertex UMETA(ToolTip = "Use the mesh's vertex normal as the base normal."),
};

UENUM()
enum class ETangentSource : uint8
{
	Explicit UMETA(ToolTip = "Explicitly define tangent and bitangent inputs."),
	Synthesized UMETA(ToolTip = "Synthesized the tangent and bitangent with screen-space derivatives based on input UVs, base normal, and position."),
	Vertex UMETA(ToolTip = "Read the tangent and bitangent from the mesh's vertex data. More accurate than Synthesized tangents, but only works correctly if your input UVs are based on the UV0 of the mesh!"),
};

UENUM()
enum class EPositionSource : uint8
{
	Explicit UMETA(ToolTip = "Explicitly position to use for internal calculations."),
	World UMETA(ToolTip = "Use the world position if a position is needed for any internal calculations."),
};

UENUM()
enum class ECameraVectorSource : uint8
{
	Explicit UMETA(ToolTip = "Explicitly camera vector to use for internal calculations."),
	World UMETA(ToolTip = "Use the world camera vector if it's needed for any internal calculations."),
};

struct FTextureSetSampleContext
{
	EBaseNormalSource BaseNormalSource;
	ETangentSource TangentSource;
	EPositionSource PositionSource;
	ECameraVectorSource CameraVectorSource;
};