#pragma once

#include "AstNode.h"

namespace lethe
{

class LETHE_API AstInitializerList : public AstNode
{
	LETHE_BUCKET_ALLOC(AstInitializerList)

public:
	LETHE_AST_NODE(AstInitializerList)

	typedef AstNode Super;

	explicit AstInitializerList(const TokenLocation &nloc) : AstNode(AST_INITIALIZER_LIST, nloc) {}

	bool GenInitializerList(CompiledProgram &p, QDataType qdt, Int ofs, bool global) override;
	bool IsCompleteInitializerList(CompiledProgram &p, QDataType qdt) const override;
	bool IsInitializerConst(const CompiledProgram &p, QDataType qdt) const override;

	void CopyTo(AstNode *n) const override;

	// designator names, none if empty
	struct Designator
	{
		String name;
		// offset has to be resolved later
		Int offset = -1;
	};

	Array<Designator> designators;

private:
	bool GenInitializeElem(CompiledProgram &p, AstNode *n, QDataType elem, Int ofs, bool global);
	bool IsElemConst(const CompiledProgram &p, AstNode *n, QDataType elem) const;
	bool IsCompleteInitializedElem(CompiledProgram &p, AstNode *n, QDataType elem) const;
};

}
