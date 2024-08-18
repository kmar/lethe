#include "AstTypeDef.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeDef

bool AstTypeDef::BeginCodegen(CompiledProgram &p)
{
	// check for recursion!
	auto *n = nodes[0];

	while (n)
	{
		if (!n->target)
			break;

		auto tmp = const_cast<AstNode *>(n->target->GetTypeNode());
		n = tmp ? tmp : n->target;

		if (!n || n->type != AST_TYPEDEF)
			break;

		if (n == this || n->target == this)
			return p.Error(this, "typedef recursion");

		n = n->nodes[0];
	}

	return Super::BeginCodegen(p);
}

bool AstTypeDef::IsElemType() const
{
	return nodes[0]->IsElemType();
}

const AstNode *AstTypeDef::GetTypeNode() const
{
	if (qualifiers & AST_Q_TYPEDEF_LOCK)
		return nullptr;

	qualifiers |= AST_Q_TYPEDEF_LOCK;
	auto res = nodes[0]->GetTypeNode();
	qualifiers &= ~AST_Q_TYPEDEF_LOCK;

	return res;
}

AstNode *AstTypeDef::GetResolveTarget() const
{
	if (qualifiers & AST_Q_TYPEDEF_LOCK)
		return nullptr;

	auto *res = nodes[0]->GetResolveTarget();

	while (res && res->type == AST_TYPEDEF)
	{
		qualifiers |= AST_Q_TYPEDEF_LOCK;
		res = res->nodes[0]->GetResolveTarget();
		qualifiers &= ~AST_Q_TYPEDEF_LOCK;
	}

	return res;
}

QDataType AstTypeDef::GetTypeDesc(const CompiledProgram &p) const
{
	return nodes[0]->GetTypeDesc(p);
}

bool AstTypeDef::TypeGenDef(CompiledProgram &p)
{
	if (nodes[0]->target)
		LETHE_RET_FALSE(nodes[0]->target->TypeGen(p));

	return Super::TypeGenDef(p);
}

bool AstTypeDef::TypeGen(CompiledProgram &p)
{
	if (nodes[0]->target)
		LETHE_RET_FALSE(nodes[0]->target->TypeGen(p));

	return Super::TypeGen(p);
}

AstNode::ResolveResult AstTypeDef::Resolve(const ErrorHandler &eh)
{
	return Super::Resolve(eh);
}

}
