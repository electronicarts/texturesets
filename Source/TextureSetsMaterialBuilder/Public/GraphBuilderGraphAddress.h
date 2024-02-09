// (c) Electronic Arts. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"

class MaterialExpression;

struct TEXTURESETSMATERIALBUILDER_API FGraphBuilderGraphAddress
{
public:
	FGraphBuilderGraphAddress()
		: Expression(nullptr)
		, Index(0)
	{}

	FGraphBuilderGraphAddress(UMaterialExpression* Expression, uint32 Index)
		: Expression(Expression)
		, Index(Index)
	{}

	uint32 GetHash() const { return HashCombine(GetTypeHash(Expression), GetTypeHash(Index)); }

	UMaterialExpression* GetExpression() const { return Expression; }
	int32 GetIndex() const { return Index; }

	bool operator==(FGraphBuilderGraphAddress const& Other) const
	{ 
		return Expression == Other.Expression && Index == Other.Index;
	} 

private:
	UMaterialExpression* Expression;
	int32 Index;
};

FORCEINLINE uint32 GetTypeHash(const FGraphBuilderGraphAddress& Address)
{
	return Address.GetHash();
}

// Address of an input pin to a material expression
struct TEXTURESETSMATERIALBUILDER_API FGraphBuilderInputAddress : public FGraphBuilderGraphAddress
{
public:
	FGraphBuilderInputAddress()
		: FGraphBuilderGraphAddress()
	{}

	FGraphBuilderInputAddress(UMaterialExpression* Expression, uint32 Index)
		: FGraphBuilderGraphAddress(Expression, Index)
	{
		check(IsValid());
	}

	bool IsValid() const;
};

// Address of an output pin of a material expression
struct TEXTURESETSMATERIALBUILDER_API FGraphBuilderOutputAddress : public FGraphBuilderGraphAddress
{
public:
	FGraphBuilderOutputAddress()
		: FGraphBuilderGraphAddress()
	{}

	FGraphBuilderOutputAddress(UMaterialExpression* Expression, uint32 Index)
		: FGraphBuilderGraphAddress(Expression, Index)
	{
		check(IsValid());
	}

	bool IsValid() const;
};
#endif // WITH_EDITOR
