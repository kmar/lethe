#pragma once

#include <Lethe/Script/Ast/AstNode.h>

namespace lethe
{

class LETHE_API AstDotOp : public AstNode
{
	LETHE_BUCKET_ALLOC(AstDotOp)

public:
	LETHE_AST_NODE(AstDotOp)

	typedef AstNode Super;

	AstDotOp(const TokenLocation &nloc) : Super(AST_OP_DOT, nloc) {}

	enum
	{
		IDX_LEFT,
		IDX_RIGHT
	};

	bool FoldConst(const CompiledProgram &p) override;

	bool ResolveNode(const ErrorHandler &e) override;

	AstNode *GetResolveTarget() const override;

	bool CodeGen(CompiledProgram &p) override;
	bool CodeGenRef(CompiledProgram &p, bool allowConst = 0, bool derefPtr = 0) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	AstNode *FindSymbolNode(String &sname, const NamedScope *&nscope) const override;
	AstSymbol *FindVarSymbolNode() override;

	const AstNode *GetTypeNode() const override;

	bool TypeGen(CompiledProgram &p) override;

	bool CanPassByReference(const CompiledProgram &p) const override
	{
		return nodes[IDX_RIGHT]->CanPassByReference(p);
	}

	void LockProp();
	void UnlockProp();

private:
	Int refPropLock = 0;
};


}
