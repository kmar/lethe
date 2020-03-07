#pragma once

#include "AstNode.h"

namespace lethe
{

class LETHE_API AstThis : public AstNode
{
public:
	SCRIPT_AST_NODE(AstThis)

	typedef AstNode Super;

	explicit AstThis(const TokenLocation &nloc) : AstNode(AST_THIS, nloc) {}

	bool ResolveNode(const ErrorHandler &e) override;
	bool CodeGen(CompiledProgram &p) override;
	bool CodeGenRef(CompiledProgram &p, bool allowConst = false, bool derefPtr = false) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	const AstNode *GetTypeNode() const override;

	bool CanPassByReference(const CompiledProgram &) const override
	{
		return true;
	}
};

}
