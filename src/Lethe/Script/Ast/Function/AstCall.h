#pragma once

#include "../AstNode.h"

namespace lethe
{

class AstFuncBase;

LETHE_API_BEGIN

class LETHE_API AstCall : public AstNode
{
	LETHE_BUCKET_ALLOC(AstCall)

public:
	LETHE_AST_NODE(AstCall)

	typedef AstNode Super;

	explicit AstCall(const TokenLocation &nloc) : Super(AST_CALL, nloc), forceFunc(nullptr) {}

	ResolveResult Resolve(const ErrorHandler &e) override;
	const AstNode *GetTypeNode() const override;

	const AstNode *GetContextTypeNode(const AstNode *node) const override;

	bool CodeGen(CompiledProgram &p) override;
	bool CodeGenRef(CompiledProgram &p, bool allowConst = false, bool derefPtr = false) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	AstNode *GetResolveTarget() const override;

	bool TypeGen(CompiledProgram &p) override;

	bool CanPassByReference(const CompiledProgram &p) const override
	{
		return nodes[0]->GetTypeDesc(p).IsReference();
	}

	// arity must be 1 for unary, 2 for binary
	static bool CallOperator(Int arity, CompiledProgram &p, AstNode *n, AstNode *op, bool isRef = false);

	// get function base node or null on error
	const AstFuncBase *GetFuncBase() const;

	AstNode *forceFunc;

	// named arguments, can be empty
	Array<String> namedArgs;

	void CopyTo(AstNode *n) const override;

protected:
	struct TempArg
	{
		// stack offset in bytes
		Int offset;
		// size in bytes
		Int size;
	};

	const AstNode *GetTypeNodeFunc() const;

	bool CodeGenCommon(CompiledProgram &p, bool keepRefs, bool derefPtr);
	bool CodeGenIntrinsic(CompiledProgram &p, AstNode *fdef);

	const AstNode *FindEnclosingFunction() const;

	// find function definition
	AstNode *FindFunction(String &fname) const;
	// find special ADL scope for elementary type
	NamedScope *FindSpecialADLScope(const NamedScope *baseScope, const StringRef &nname) const;

	Int GenTempCopy(CompiledProgram &p, Int resultBaseOffset, Int resultWords, bool resultZeroed, Int actual, const Array<TempArg> &tempArgs);

	void CheckDeprecatedCall(CompiledProgram &p, AstNode *fdef, const Attributes *attrs);
};

LETHE_API_END

}
