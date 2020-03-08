#pragma once

#include "AstText.h"

namespace lethe
{

class LETHE_API AstImport : public AstText
{
public:
	LETHE_AST_NODE(AstImport)

	typedef AstText Super;

	explicit AstImport(const String &ntext, const TokenLocation &nloc) :
		AstText(ntext, AST_IMPORT, nloc)
	{
	}
};

}
