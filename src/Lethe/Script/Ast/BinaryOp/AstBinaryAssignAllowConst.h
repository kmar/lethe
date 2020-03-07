#pragma once

#include "AstBinaryAssign.h"

namespace lethe
{

// special; allow assignment to const objects (used in ctor initialized vars)
class LETHE_API AstBinaryAssignAllowConst : public AstBinaryAssign
{
public:
	SCRIPT_AST_NODE(AstBinaryAssignAllowConst)

	typedef AstBinaryAssign Super;

	explicit AstBinaryAssignAllowConst(const TokenLocation &nloc) : Super(nloc) {}

	bool CodeGen(CompiledProgram &p) override;
};


}
