#pragma once

#include "AstCustomType.h"
#include <Lethe/Script/TypeInfo/Attributes.h>

namespace lethe
{

class AstTypeDef;

class LETHE_API AstTypeStruct : public AstCustomType
{
	LETHE_BUCKET_ALLOC(AstTypeStruct)

public:
	LETHE_AST_NODE(AstTypeStruct)

	typedef AstCustomType Super;

	enum
	{
		IDX_NAME = 0,
		IDX_BASE = 1
	};

	AstTypeStruct(const TokenLocation &nloc)
		: Super(AST_STRUCT, nloc)
		, minAlign(0)
	{
	}

	// get custom constructor node, returns 0 if none
	AstFunc *GetCustomCtor();

	// get custom destructor node, returns 0 if none
	AstFunc *GetCustomDtor();

	bool BeginCodegen(CompiledProgram &p) override;

	bool TypeGen(CompiledProgram &p) override;
	bool CodeGenComposite(CompiledProgram &p) override;
	bool ResolveNode(const ErrorHandler &e) override;
	ResolveResult Resolve(const ErrorHandler &e) override;

	bool FoldConst(const CompiledProgram &p) override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;

	void CopyTo(AstNode *n) const override;

	bool IsTemplateArg(const StringRef &sr) const;

	struct TemplateArg
	{
		String name;
		AstTypeDef *typedefNode;
	};

	const TemplateArg *FindTemplateArg(const StringRef &sr) const;

	StackArray<TemplateArg, 4> templateArgs;

	// for template instances
	String overrideName;
	// alignment expression
	UniquePtr<AstNode> alignExpr;
	// minimum alignment; 0 = auto
	Int minAlign;

	// note: not cloned, just temporary
	StackArray<AstNode *, 16> postponeTypeGen;

	SharedPtr<Attributes> attributes;
};

}
