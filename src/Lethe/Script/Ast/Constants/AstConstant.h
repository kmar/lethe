#pragma once

#include <Lethe/Script/Ast/AstNode.h>

namespace lethe
{

class AstEnumItem;
class AstSymbol;

// constants or literals
class LETHE_API AstConstant : public AstNode
{
public:
	LETHE_AST_NODE(AstConstant)

	typedef AstNode Super;

	AstConstant(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	AstNode *ConvertConstTo(DataTypeEnum dte, const CompiledProgram &p) override;
	Int ToBoolConstant(const CompiledProgram &p) override;

	bool IsZeroConstant(const CompiledProgram &p) const override;

private:
	Int ToBoolConstantInternal(const CompiledProgram &p) const;
};

class LETHE_API AstConstEnumBase : public AstConstant
{
	LETHE_BUCKET_ALLOC(AstConstEnumBase)
public:
	LETHE_AST_NODE(AstConstEnumBase)

	typedef AstConstant Super;

	AstConstEnumBase(AstNodeType ntype, const TokenLocation &nloc, const DataType *tref = nullptr)
		: Super(ntype, nloc)
		, typeRef(tref)
	{
	}

	void CopyTo(AstNode *n) const override;

protected:
	friend class AstEnumItem;
	friend class AstSymbol;

	const DataType *typeRef = nullptr;
};

}
