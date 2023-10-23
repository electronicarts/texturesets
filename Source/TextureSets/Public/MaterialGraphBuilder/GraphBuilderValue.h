// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "MaterialGraphBuilder/GraphBuilderGraphAddress.h"

UENUM()
enum class EGraphBuilderSharedValueType : uint8
{
	// Raw Texture coordinate (UV) without modifications
	Texcoord_Raw,
	// Texture coordinate used for doing texture reads
	Texcoord_Sampling,
	// Texture coordinate used to inform the streaming system for non-virtual textures
	Texcoord_Streaming,
	// Texture coordinate used to look up mips for texture reads
	Texcoord_Mip,
	// DDX and DDY of texture coordinates used to look up mips for texture reads
	Texcoord_DDX,
	Texcoord_DDY,
	// Index used for sampling from texture arrays
	ArrayIndex,
	// Base normal used for tangent space transforms
	BaseNormal,
	// Tangent and Bitangent used for tangent space transforms
	Tangent,
	Bitangent,
	// Per-pixel position value. Typically in world space, but also valid to be in view or object
	Position,
	// Pixel to camera vector. Should be in the same space as the position
	CameraVector
};

struct FGraphBuilderValue
{
public:
	FGraphBuilderOutputAddress Source;
	FGraphBuilderOutputAddress Reroute;
};

#endif // WITH_EDITOR
