#pragma once

#include "AstText.h"

namespace lethe
{

class LETHE_API AstNamespace : public AstText
{
public:
	SCRIPT_AST_NODE(AstNamespace)

	typedef AstText Super;

	explicit AstNamespace(const String &ntext, const TokenLocation &nloc) : Super(ntext, AST_NAMESPACE, nloc) {}

	bool ResolveNode(const ErrorHandler &e) override;
};

}
