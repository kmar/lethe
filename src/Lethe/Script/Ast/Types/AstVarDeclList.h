#pragma once

#include "AstCustomType.h"
#include <Lethe/Script/TypeInfo/Attributes.h>

namespace lethe
{

LETHE_API_BEGIN

class LETHE_API AstVarDeclList : public AstCustomType
{
	LETHE_BUCKET_ALLOC(AstVarDeclList)

public:
	LETHE_AST_NODE(AstVarDeclList)

	typedef AstCustomType Super;

	SharedPtr<Attributes> attributes;

	AstVarDeclList(const TokenLocation &nloc) : Super(AST_VAR_DECL_LIST, nloc) {}

	void LoadIfVarDecl(CompiledProgram &p) override;
};

LETHE_API_END

}
