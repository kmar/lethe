#pragma once

#include "AstCustomType.h"
#include <Lethe/Script/TypeInfo/Attributes.h>

namespace lethe
{

class LETHE_API AstVarDeclList : public AstCustomType
{
	LETHE_BUCKET_ALLOC(AstVarDeclList)

public:
	SCRIPT_AST_NODE(AstVarDeclList)

	typedef AstCustomType Super;

	SharedPtr<Attributes> attributes;

	AstVarDeclList(const TokenLocation &nloc) : Super(AST_VAR_DECL_LIST, nloc) {}

	void LoadIfVarDecl(CompiledProgram &p) override;
};


}
