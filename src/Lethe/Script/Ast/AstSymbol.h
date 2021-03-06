#pragma once

#include "AstText.h"

namespace lethe
{

class LETHE_API AstSymbol : public AstText
{
public:
	LETHE_AST_NODE(AstSymbol)

	typedef AstText Super;

	explicit AstSymbol(const String &ntext, const TokenLocation &nloc) : Super(ntext, AST_IDENT, nloc) {}

	bool ResolveNode(const ErrorHandler &e) override;

	bool TypeGen(CompiledProgram &p) override;

	const AstNode *GetTypeNode() const override;

	bool CodeGen(CompiledProgram &p) override;
	bool CodeGenRef(CompiledProgram &p, bool allowConst = 0, bool derefPtr = 0) override;

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	AstNode *DerefConstant(const CompiledProgram &p) override;
	AstNode *FindSymbolNode(String &sname, const NamedScope *&nscope) const override;
	AstSymbol *FindVarSymbolNode() override;
	bool FoldConst(const CompiledProgram &p) override;
	Int ToBoolConstant(const CompiledProgram &p) override;

	bool CanPassByReference(const CompiledProgram &) const override
	{
		return (qualifiers & AST_Q_PROPERTY) == 0;
	}

	bool CallPropertyGetterLocal(CompiledProgram &p);
	bool CallPropertyGetterViaPtr(CompiledProgram &p, AstNode *root);

	static bool CallPropertySetter(CompiledProgram &p, AstNode *dnode, AstNode *snode);

private:
	bool Validate(const CompiledProgram &) const;
};


}
