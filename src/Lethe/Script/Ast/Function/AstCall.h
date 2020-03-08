#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstCall : public AstNode
{
	LETHE_BUCKET_ALLOC(AstCall)

public:
	LETHE_AST_NODE(AstCall)

	typedef AstNode Super;

	explicit AstCall(const TokenLocation &nloc) : Super(AST_CALL, nloc), forceFunc(nullptr) {}

	ResolveResult Resolve(const ErrorHandler &e) override;
	const AstNode *GetTypeNode() const override;

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

	AstNode *forceFunc;

	void CopyTo(AstNode *n) const override;

protected:
	struct TempArg
	{
		// stack offset in bytes
		Int offset;
		// size in bytes
		Int size;
	};

	bool CodeGenCommon(CompiledProgram &p, bool keepRefs, bool derefPtr);
	bool CodeGenIntrinsic(CompiledProgram &p, AstNode *fdef);

	// find function definition
	AstNode *FindFunction(String &fname) const;
	// find special ADL scope for elementary type
	NamedScope *FindSpecialADLScope(const StringRef &nname) const;

	Int GenTempCopy(CompiledProgram &p, Int resultBaseOffset, Int resultWords, bool resultZeroed, Int actual, const Array<TempArg> &tempArgs);
};


}
