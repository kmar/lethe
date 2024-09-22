#pragma once

#include "AstNode.h"

namespace lethe
{

class AstSymbol;

LETHE_API_BEGIN

class LETHE_API AstVarDecl : public AstNode
{
	LETHE_BUCKET_ALLOC(AstVarDecl)

public:
	LETHE_AST_NODE(AstVarDecl)

	typedef AstNode Super;

	AstVarDecl(const TokenLocation &nloc) : Super(AST_VAR_DECL, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGenComposite(CompiledProgram &p) override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
	AstNode *GetResolveTarget() const override;

	const AstNode *GetContextTypeNode(const AstNode *node) const override;

	static bool CallInit(CompiledProgram &p, const AstNode *varType, Int globalOfs, Int localOfs = 0);

	// used by naive local ref static analysis
	UInt modifiedCounter = 0;

	struct LiveRef
	{
		AstVarDecl *var;
		UInt counter;
	};

	StackArray<LiveRef, 4> liveRefs;

private:
	void AddLiveRefs(AstNode *n);
};

LETHE_API_END

}
