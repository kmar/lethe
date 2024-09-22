#pragma once

#include "AstUnaryOp.h"
#include "../Types/AstTypeArrayRef.h"

namespace lethe
{

LETHE_API_BEGIN

class LETHE_API AstUnaryRef : public AstUnaryOp
{
	LETHE_BUCKET_ALLOC(AstUnaryRef)
public:
	LETHE_AST_NODE(AstUnaryRef)

	enum
	{
		IDX_REF
	};

	typedef AstUnaryOp Super;

	AstUnaryRef(const TokenLocation &nloc) : Super(AST_UOP_REF, nloc)
	{
	}

	~AstUnaryRef();

	ResolveResult Resolve(const ErrorHandler &e) override;

	bool TypeGen(CompiledProgram &p) override;

	bool CodeGen(CompiledProgram &p) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	const AstNode *GetTypeNode() const override;

	// cache and create ArrayRef node (so that we can auto-resolve Slice, sigh)
	AstNode *GetArrayRefNode() const;

	bool FoldConst(const CompiledProgram &p) override;

	void CopyTo(AstNode *n) const override;

private:
	mutable UniquePtr<AstTypeArrayRef> aref;
};

LETHE_API_END

}
