#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeAuto : public AstBaseType
{
	LETHE_BUCKET_ALLOC(AstTypeAuto)
public:
	LETHE_AST_NODE(AstTypeAuto)

	typedef AstBaseType Super;

	AstTypeAuto(const TokenLocation &nloc) : Super(AST_TYPE_AUTO, nloc) {}

	ResolveResult Resolve(const ErrorHandler &e) override;

	const AstNode *GetTypeNode() const override;
	bool TypeGen(CompiledProgram &p) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	void CopyTo(AstNode *n) const override;

private:
	AstNode *GetExprNode() const;
	AstNode *GetAutoType() const;
	mutable AstNode *typeCache = nullptr;
	mutable bool lockFlag = false;
	mutable bool reportedFlag = false;
	mutable Int typeLockCount = 0;
};


}
