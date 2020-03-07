#pragma once

#include "AstTextConstant.h"

namespace lethe
{

class LETHE_API AstConstName : public AstTextConstant
{
public:
	SCRIPT_AST_NODE(AstConstName)

	typedef AstTextConstant Super;

	AstConstName(const String &ntext, const TokenLocation &nloc);

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
