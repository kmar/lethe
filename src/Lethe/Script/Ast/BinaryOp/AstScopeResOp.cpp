#include "AstScopeResOp.h"
#include "../NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstScopeResOp

AstNode *AstScopeResOp::ResolveTemplateScope(AstNode *&ntext) const
{
	const AstNode *it = this;

	while (it->parent && it->parent->type == AST_OP_SCOPE_RES)
		it = it->parent;

	ntext = const_cast<AstNode *>(it);

	return AstStaticCast<const AstScopeResOp *>(it)->ResolveScopeInternal(true);
}

AstNode *AstScopeResOp::ResolveScopeInternal(bool wantTemplate, const NamedScope *rscope) const
{
	// we have a list of idents (first can be AST_NONE is which case it refers to global namespace)
	LETHE_ASSERT(!nodes.IsEmpty());
	bool global = nodes[0]->type == AST_NONE;
	Int start = global;

	const NamedScope *scope = scopeRef;

	if (global)
	{
		// look up root global scope
		const NamedScope *tmp = scope;

		while (tmp)
		{
			if (tmp->IsGlobal())
				scope = tmp;

			tmp = tmp->parent;
		}

		LETHE_ASSERT(!scope || scope->name.IsEmpty());
	}
	else
	{
		if (rscope)
			scope = rscope;
	}

	return ResolveScope(scope, start, AST_Q_TEMPLATE*wantTemplate);
}

bool AstScopeResOp::ResolveNode(const ErrorHandler &)
{
	if (type != AST_OP_SCOPE_RES)
		return true;

	const NamedScope *rscope = nullptr;

	if (parent && parent->type == AST_OP_DOT && parent->nodes[0] != this)
	{
		if (!(parent->nodes[0]->flags & AST_F_RESOLVED))
			return true;

		rscope = parent->symScopeRef;

		if (!rscope || !rscope->IsComposite())
			return true;
	}

	auto *res = !target ?
		ResolveScopeInternal(false, rscope) :
		ResolveScope(target->scopeRef, nodes.GetSize()-1);

	if (!res)
		return true;

	if (rscope)
	{
		// make sure it's valid
		const NamedScope *tscope = res->scopeRef;

		while (tscope && !tscope->IsComposite())
			tscope = tscope->parent;

		if (!tscope || !tscope->IsBaseOf(rscope))
			return true;
	}

	if (target && !res)
		return true;

	AstNode *newNode = nodes[nodes.GetSize()-1];
	res->flags |= AST_F_REFERENCED;
	newNode->target = res;
	newNode->qualifiers |= AST_Q_NON_VIRT;
	newNode->flags |= AST_F_RESOLVED;
	nodes.Pop();
	ClearNodes();

	// FIXME: hacky
	text = AstStaticCast<const AstText *>(newNode)->text;
	type = newNode->type;
	location = newNode->location;
	qualifiers |= newNode->qualifiers;
	flags |= newNode->flags;
	target = newNode->target;

	// copy property flag from target
	if (target)
		qualifiers |= target->qualifiers & AST_Q_PROPERTY;

	symScopeRef = target->scopeRef;

	// copy underlying nodes because of templates
	Swap(nodes, newNode->nodes);

	for (auto *it : nodes)
		it->parent = this;

	delete newNode;

	return true;
}

AstNode *AstScopeResOp::ResolveScope(const NamedScope *scope, Int idx, ULong stopMask) const
{
	LETHE_RET_FALSE(scope && idx < nodes.GetSize());

	LETHE_ASSERT(nodes[idx]->type == AST_IDENT);
	auto stext = AstStaticCast<const AstText *>(nodes[idx])->text;

	bool isSuper = stext == "super";

	if (idx+1 == nodes.GetSize())
	{
		// just look up member
		AstNode *res = scope->FindSymbol(stext);

		if (res)
			return res;

		Int ridx = scope->namedScopes.FindIndex(stext);

		if (ridx >= 0)
			return scope->namedScopes.GetValue(ridx)->node;

		return nullptr;
	}

	if (isSuper)
	{
		while (scope && !scope->IsComposite())
			scope = scope->parent;

		if (scope)
			scope = scope->base;
	}

	while (scope)
	{
		if (isSuper || scope->name == stext || scope->nameAlias == stext)
		{
			// try to go deeper

			if (scope->node && (scope->node->qualifiers & stopMask))
				return scope->node;

			AstNode *res = ResolveScope(scope, idx+1, stopMask);

			if (res)
				return res;
		}
		else
		{
			Int nidx = scope->namedScopes.FindIndex(stext);

			if (nidx >= 0)
			{
				auto *tmp = scope->namedScopes.GetValue(nidx).Get();

				if (tmp && tmp->node && (tmp->node->qualifiers & stopMask))
					return tmp->node;

				AstNode *res = ResolveScope(tmp, idx+1, stopMask);

				if (res)
					return res;

			}

			// could be a typedef so try to look up member
			auto *tmp = scope->FindSymbol(stext);

			if (tmp && tmp->type == AST_TYPEDEF)
			{
				tmp = tmp->GetResolveTarget();

				if (tmp)
				{
					if (tmp->qualifiers & stopMask)
						return tmp;

					auto *nextScope = tmp->scopeRef;

					if (tmp->qualifiers & AST_Q_ENUM_CLASS)
					{
						// special handling for enum classes...
						LETHE_ASSERT(tmp->type == AST_ENUM);
						auto eidx = tmp->scopeRef->namedScopes.FindIndex(AstStaticCast<AstText *>(tmp->nodes[0])->text);
						nextScope = tmp->scopeRef->namedScopes.GetValue(eidx);
					}

					// try to go deeper
					AstNode *res = ResolveScope(nextScope, idx+1, stopMask);

					if (res)
						return res;
				}
			}
		}

		scope = isSuper ? scope->base : scope->parent;
	}

	return nullptr;
}


}
