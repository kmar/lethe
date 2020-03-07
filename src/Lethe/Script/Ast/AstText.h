#pragma once

#include "AstNode.h"

namespace lethe
{

class LETHE_API AstText : public AstNode
{
	LETHE_BUCKET_ALLOC(AstText)

public:
	SCRIPT_AST_NODE(AstText)

	typedef AstNode Super;

	explicit AstText(const String &ntext, AstNodeType ntype, const TokenLocation &nloc) :
		AstNode(ntype, nloc), text(ntext) {}

	String text;

	// get fully qualified text
	String GetQText(CompiledProgram &p) const;
	String GetQTextSlow() const;

	String GetTextRepresentation() const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;

	AstNode *ResolveTemplateScope(AstNode *&ntext) const override;

	void CopyTo(AstNode *n) const override;
};

}
