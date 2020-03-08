#pragma once

#include <Lethe/Script/Ast/AstNode.h>

namespace lethe
{

class LETHE_API AstSubscriptOp : public AstNode
{
public:
	LETHE_AST_NODE(AstSubscriptOp)

	typedef AstNode Super;

	AstSubscriptOp(const TokenLocation &nloc) : Super(AST_OP_SUBSCRIPT, nloc) {}

	AstNode *GetResolveTarget() const override;
	const AstNode *GetTypeNode() const override;

	bool CodeGen(CompiledProgram &p) override;
	bool CodeGenRef(CompiledProgram &p, bool allowConst = 0, bool derefPtr = 0) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	AstNode *FindSymbolNode(String &sname, const NamedScope *&nscope) const override;
	AstNode *FindVarSymbolNode() override;

	bool CanPassByReference(const CompiledProgram &) const override
	{
		return true;
	}

private:
	bool CodeGenSubscript(CompiledProgram &p, bool store, bool allowConst, bool derefPtr = 0);
};


}
