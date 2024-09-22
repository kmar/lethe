#pragma once

#include "AstTypeArray.h"
#include "AstTypeArrayRef.h"

namespace lethe
{

LETHE_API_BEGIN

class LETHE_API AstTypeDynamicArray : public AstTypeArray
{
	LETHE_BUCKET_ALLOC(AstTypeDynamicArray)
public:
	LETHE_AST_NODE(AstTypeDynamicArray)

	enum
	{
		IDX_TYPE
	};

	typedef AstTypeArray Super;

	AstTypeDynamicArray(const TokenLocation &nloc) : Super(nloc)
	{
		type = AST_TYPE_DYNAMIC_ARRAY;
		flags |= AST_F_SKIP_CGEN;
	}

	~AstTypeDynamicArray();

	ResolveResult Resolve(const ErrorHandler &e) override;

	bool TypeGen(CompiledProgram &p) override;

	// cache and create ArrayRef node (so that we can auto-resolve Slice, sigh)
	AstNode *GetArrayRefNode();

	void CopyTo(AstNode *n) const override;

private:
	UniquePtr<AstTypeArrayRef> aref;
};

LETHE_API_END

}
