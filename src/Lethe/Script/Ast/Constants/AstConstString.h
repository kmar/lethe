#pragma once

#include "AstTextConstant.h"

namespace lethe
{

class LETHE_API AstConstString : public AstTextConstant
{
public:
	SCRIPT_AST_NODE(AstConstString)

	typedef AstTextConstant Super;

	AstConstString(const String &ntext, const TokenLocation &nloc) : Super(ntext, AST_CONST_STRING, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	bool CodeGenRef(CompiledProgram &p, bool allowConst, bool derefPtr = 0) override;
	const AstNode *GetTypeNode() const override;

	bool CanPassByReference(const CompiledProgram &) const override
	{
		return true;
	}
};


}
