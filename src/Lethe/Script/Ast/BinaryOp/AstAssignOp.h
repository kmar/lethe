#pragma once

#include "AstBinaryOp.h"

namespace lethe
{

class LETHE_API AstAssignOp : public AstBinaryOp
{
public:
	SCRIPT_AST_NODE(AstAssignOp)
	typedef AstBinaryOp Super;

	AstAssignOp(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc) {}

	bool IsConstExpr() const override
	{
		return false;
	}

	bool CodeGen(CompiledProgram &p) override;
	bool CodeGenCommon(CompiledProgram &p, bool needConv = true, bool asRef = false, bool allowConst = false);
	bool CodeGenRef(CompiledProgram &p, bool allowConst = 0, bool derefPtr = 0) override;

	static bool CodeGenPrepareAssign(CompiledProgram &p, QDataType &rhs,
									 bool &structOnStack, Int &ssize);
	static bool CodeGenDoAssign(AstNode *n, CompiledProgram &p, const QDataType &dst, const QDataType &rhs,
								bool pop, bool structOnStack, Int ssize);

	bool BeginCodegen(CompiledProgram &p) override;

private:
	const char *GetOpName() const;

	// -1 => not found, 0 => error, 1 => ok
	Int CodeGenOperator(CompiledProgram &p);
};


}
