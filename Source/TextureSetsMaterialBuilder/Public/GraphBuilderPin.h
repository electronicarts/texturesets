// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Materials/MaterialExpression.h"

struct TEXTURESETSMATERIALBUILDER_API FGraphBuilderPin
{
public:
	FGraphBuilderPin()
		: Expression(nullptr)
		, Index(0)
	{}

	FGraphBuilderPin(UMaterialExpression* Expression, uint32 Index)
		: Expression(Expression)
		, Index(Index)
	{}

	uint32 GetHash() const { return HashCombine(GetTypeHash(Expression), GetTypeHash(Index)); }

	UMaterialExpression* GetExpression() const { return Expression; }
	int32 GetIndex() const { return Index; }

	bool operator==(FGraphBuilderPin const& Other) const
	{ 
		return Expression == Other.Expression && Index == Other.Index;
	} 

private:
	UMaterialExpression* Expression;
	int32 Index;
};

FORCEINLINE uint32 GetTypeHash(const FGraphBuilderPin& Address)
{
	return Address.GetHash();
}

struct TEXTURESETSMATERIALBUILDER_API FGraphBuilderInputPin : public FGraphBuilderPin
{
public:
	FGraphBuilderInputPin()
		: FGraphBuilderPin()
	{}

	FGraphBuilderInputPin(UMaterialExpression* Expression, uint32 Index)
		: FGraphBuilderPin(Expression, Index)
	{
		check(IsValid());
	}

	bool IsValid() const;
};

struct TEXTURESETSMATERIALBUILDER_API FGraphBuilderOutputPin : public FGraphBuilderPin
{
public:
	FGraphBuilderOutputPin()
		: FGraphBuilderPin()
	{}

	FGraphBuilderOutputPin(UMaterialExpression* Expression, uint32 Index)
		: FGraphBuilderPin(Expression, Index)
	{
		check(IsValid());
	}

	bool IsValid() const;
};
