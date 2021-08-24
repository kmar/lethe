#pragma once

#include "../AstText.h"

namespace lethe
{

class LETHE_API AstLabel : public AstText
{
	LETHE_BUCKET_ALLOC(AstLabel)
	friend class AstGoto;
public:
	LETHE_AST_NODE(AstLabel)

	typedef AstText Super;

	AstLabel(const String &ntext, const TokenLocation &nloc);

	bool BeginCodegen(CompiledProgram &) override;
	bool CodeGen(CompiledProgram &p) override;

	void CopyTo(AstNode *n) const override;

	Int pc;
	Int varOfsBase;
	Int localVarSize;
	Int deferredSize;
private:
	StackArray<Int, 1> forwardTargets;
};


}
