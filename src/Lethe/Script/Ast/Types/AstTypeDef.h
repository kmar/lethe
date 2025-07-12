#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstTypeDef : public AstNode
{
public:
	LETHE_AST_NODE(AstTypeDef)

	typedef AstNode Super;

	AstTypeDef(const TokenLocation &nloc) : Super(AST_TYPEDEF, nloc) {}

	bool BeginCodegen(CompiledProgram &p) override;

	const AstNode *GetTypeNode() const override;
	AstNode *GetResolveTarget() const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;

	bool IsElemType() const override;

	bool TypeGenDef(CompiledProgram &p) override;
	bool TypeGen(CompiledProgram &p) override;

	ResolveResult Resolve(const ErrorHandler &e) override;

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
};

// virtual alias for template params
class LETHE_API AstTypeDefTemplateParam : public AstTypeDef
{
public:
	LETHE_AST_NODE(AstTypeDefTemplateParam)

	typedef AstTypeDef Super;

	AstTypeDefTemplateParam(const TokenLocation &nloc) : Super(nloc) {}

	bool GetTemplateTypeText(StringBuilder &) const override {return false;}
};

}
