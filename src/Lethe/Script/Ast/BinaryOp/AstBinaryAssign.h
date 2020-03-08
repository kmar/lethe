#pragma once

#include "AstAssignOp.h"

namespace lethe
{

class LETHE_API AstBinaryAssign : public AstAssignOp
{
public:
	LETHE_AST_NODE(AstBinaryAssign)

	typedef AstAssignOp Super;

	explicit AstBinaryAssign(const TokenLocation &nloc) : Super(AST_OP_ASSIGN, nloc) {}

	bool CodeGen(CompiledProgram &p) override;
	bool CodeGenRef(CompiledProgram &p, bool allowConst, bool derefPtr = false) override;

	AstNode *GetResolveTarget() const override;

	bool CanPassByReference(const CompiledProgram &) const override {return false;}

protected:
	bool CodeGenMaybeConst(CompiledProgram &p, bool allowConst);
};


}
