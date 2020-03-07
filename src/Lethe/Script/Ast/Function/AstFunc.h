#pragma once

#include <Lethe/Script/Ast/Types/AstFuncBase.h>

namespace lethe
{

class AstCall;

class LETHE_API AstFunc : public AstFuncBase
{
	LETHE_BUCKET_ALLOC(AstFunc)

public:
	SCRIPT_AST_NODE(AstFunc)

	enum
	{
		IDX_NAME = 1,
		IDX_ARGS = 2,
		IDX_BODY = 3,
	};

	typedef AstFuncBase Super;

	AstFunc(const TokenLocation &nloc) : Super(AST_FUNC, nloc), vtblIndex(-1) {}

	bool TypeGen(CompiledProgram &p) override;
	bool CodeGen(CompiledProgram &p) override;
	bool CodeGenGlobalCtorStatic(CompiledProgram &p) override;

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	void AddForwardRef(Int handle);

	inline AstNode *GetArgs() const override
	{
		return nodes[IDX_ARGS];
	}

	const AstNode *GetTypeNode() const override;

	ResolveResult Resolve(const ErrorHandler &e) override;

	bool ValidateSignature(const AstFunc &o, const CompiledProgram &p) const;
	bool ValidateADLCall(const AstCall &o) const;

	// for methods only
	Int vtblIndex;

	void CopyTo(AstNode *n) const override;

private:
	Array< Int > forwardRefs;
	QDataType typeRef;

	bool ValidateStaticInitSignature(CompiledProgram &p) const;
	bool AnalyzeFlow(CompiledProgram &p, Int startPC) const;
};


}
