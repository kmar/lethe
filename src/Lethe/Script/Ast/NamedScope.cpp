#include "NamedScope.h"
#include "AstNode.h"
#include "AstText.h"
#include "Function/AstFunc.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Stack.h>
#include <Lethe/Script/Compiler/Compiler.h>

namespace lethe
{

// NamedScope

LETHE_BUCKET_ALLOC_DEF(NamedScope)

NamedScope::NamedScope()
	: parent(nullptr)
	, base(nullptr)
	, type(NSCOPE_NONE)
	, node(nullptr)
	, chkStkIndex(-1)
	, varOfs(0)
	, varSize(0)
	, maxVarAlign(0)
	, maxVarSize(0)
	, resultPtr(nullptr)
	, needExtraScope(false)
	, ctorDefined(false)
	, blockThis(0)
	, deferredTop(0x7fffffff)
{
}

NamedScope::NamedScope(NamedScopeType ntype)
	: parent(nullptr)
	, base(nullptr)
	, type(ntype)
	, node(nullptr)
	, chkStkIndex(-1)
	, varOfs(0)
	, varSize(0)
	, maxVarAlign(0)
	, maxVarSize(0)
	, resultPtr(nullptr)
	, needExtraScope(false)
	, ctorDefined(false)
	, blockThis(0)
	, deferredTop(0x7fffffff)
{
}

void NamedScope::ResetDeferredTop()
{
	deferredTop = 0x7fffffff;
}

NamedScope *NamedScope::Add(NamedScope *nsc)
{
	LETHE_ASSERT(!nsc->parent);
	LETHE_ASSERT(nsc->name.IsEmpty());
	nsc->parent = this;
	scopes.Add(nsc);
	return nsc;
}

void NamedScope::GetFullScopeName(StringBuilder &sb) const
{
	const auto *tmp = this;

	StackArray<String, 64> scopeList;

	while (tmp)
	{
		if (!tmp->name.IsEmpty() && (tmp->type == NSCOPE_STRUCT || tmp->type == NSCOPE_CLASS || tmp->type == NSCOPE_NAMESPACE))
			scopeList.Add(tmp->name);

		tmp = tmp->parent;
	}

	for (Int i=scopeList.GetSize()-1; i >= 0; i--)
	{
		sb += scopeList[i];

		if (i > 0)
			sb += "::";
	}
}

const NamedScope *NamedScope::FindFunctionScope() const
{
	const NamedScope *res = this;

	while (res && res->type != NSCOPE_FUNCTION)
		res = res->parent;

	return res;
}

const NamedScope *NamedScope::FindThis(bool allowStatic) const
{
	if (blockThis)
		return nullptr;

	const NamedScope *res = this;

	while (res && !res->IsComposite())
	{
		if (!allowStatic && res->type == NSCOPE_FUNCTION && res->node && (res->node->qualifiers & AST_Q_STATIC))
			return nullptr;

		res = res->parent;
	}

	return res;
}

bool NamedScope::IsBaseOf(const NamedScope *nscope) const
{
	const NamedScope *res = nscope;

	while (res)
	{
		if (res == this)
			break;

		res = res->base;
	}

	return res != 0;
}

bool NamedScope::IsParentOf(const NamedScope *nscope) const
{
	const NamedScope *res = nscope;

	while (res)
	{
		if (res == this)
			break;

		res = res->parent;
	}

	return res != 0;
}

bool NamedScope::IsConstMethod() const
{
	const NamedScope *tmp = this;

	while (tmp)
	{
		if (tmp->type == NSCOPE_FUNCTION)
		{
			auto q = tmp->node->qualifiers;

			if ((q & (AST_Q_METHOD | AST_Q_CONST)) == (AST_Q_METHOD | AST_Q_CONST))
				return true;

			break;
		}

		tmp = tmp->parent;
	}

	return false;
}

AstNode *NamedScope::FindLabel(const String &sname) const
{
	const auto *scope = this;

	while (scope)
	{
		auto it = scope->labels.Find(sname);

		if (it != scope->labels.End())
			return it->value;

		if (scope->type == NSCOPE_FUNCTION)
			break;

		scope = scope->parent;
	}

	return nullptr;
}

AstNode *NamedScope::FindSymbol(const StringRef &sname, bool chainbase, bool chainparent) const
{
	// because of templates
	if (nameAlias == sname)
		return node;

	Int idx = members.FindIndex(sname);

	if (idx >= 0)
		return members.GetValue(idx);

	if (chainbase && base)
		return base->FindSymbol(sname, chainbase, chainparent);

	if (chainparent && parent)
		return parent->FindSymbol(sname, chainbase, chainparent);

	return nullptr;
}

AstNode *NamedScope::FindSymbolFull(const String &sname, const NamedScope *&nscope,
								bool baseOnly) const
{
	nscope = this;

	while (nscope)
	{
		// because of templates
		if (nscope->nameAlias == sname)
			return nscope->node;

		Int idx = nscope->members.FindIndex(sname);

		if (idx >= 0)
			return nscope->members.GetValue(idx);

		// look up base chain
		const NamedScope *bscope = nscope->base;

		while (bscope)
		{
			idx = bscope->members.FindIndex(sname);

			if (idx >= 0)
			{
				nscope = bscope;
				return bscope->members.GetValue(idx);
			}

			idx = bscope->namedScopes.FindIndex(sname);

			if (idx >= 0)
			{
				auto res = bscope->namedScopes.GetValue(idx)->node;

				if (res)
					return res;
			}

			bscope = bscope->base;
		}

		if (baseOnly)
			break;

		idx = nscope->namedScopes.FindIndex(sname);

		if (idx >= 0)
		{
			nscope = nscope->namedScopes.GetValue(idx);
			AstNode *res = nscope->node;

			if (res)
				return res;
		}

		nscope = nscope->parent;
	}

	return nullptr;
}

AstNode *NamedScope::FindOperator(const CompiledProgram &p, const char *opName, const QDataType &ntype) const
{
	AstNode *res = nullptr;

	StackArray<AstNode *, 64> candidates;

	for (auto &&op : operators)
	{
		auto args = op->nodes[AstFunc::IDX_ARGS];

		if (args->nodes.GetSize() != 1)
			continue;

		auto arg = args->nodes[0]->GetTypeDesc(p);

		if (ntype.IsConst() && arg.IsReference() && !arg.IsConst())
			continue;

		if (&arg.GetType() != &ntype.GetType())
			continue;

		if (AstStaticCast<const AstText *>(op->nodes[AstFunc::IDX_NAME])->text != opName)
			continue;

		candidates.Add(op);
	}

	// one last thing to solve: if we have more candidates where one can grab by non-const ref and another by const ref, pick the non-const ref
	// FIXME: this is ugly...
	for (Int i=1; i<candidates.GetSize(); i++)
	{
		auto *iargs = candidates[i]->nodes[AstFunc::IDX_ARGS];
		auto iarg0 = iargs->nodes[0]->GetTypeDesc(p);

		// skip non-ref overloads
		if (!iarg0.IsReference())
			continue;

		for (Int j=0; j<i; j++)
		{
			auto *jargs = candidates[j]->nodes[AstFunc::IDX_ARGS];
			auto jarg0 = jargs->nodes[0]->GetTypeDesc(p);

			// try to match type and ref-type
			if (&iarg0.GetType() != &jarg0.GetType())
				continue;

			if (iarg0.IsReference() != jarg0.IsReference())
				continue;

			// keep the one with non-const reference
			auto iconst = iarg0.IsConst() + 0;
			auto jconst = jarg0.IsConst() + 0;

			if (iconst > jconst)
				candidates.EraseIndex(i);
			else
				candidates.EraseIndex(j--);

			// erase only once, abort search
			i = candidates.GetSize();
			break;
		}
	}

	if (!candidates.IsEmpty())
		res = candidates[0];

	if (candidates.GetSize() > 1)
	{
		res = nullptr;

		Int idx = 1;

		for (auto *it : candidates)
			p.Error(it, String::Printf("overload candidate #%d", idx++));
	}

	return res;
}

AstNode *NamedScope::FindOperator(const CompiledProgram &p, const char *opName, const QDataType &ltype, const QDataType &rtype) const
{
	AstNode *res = nullptr;

	StackArray<AstNode *, 64> candidates;

	for (auto &&op : operators)
	{
		auto *args = op->nodes[AstFunc::IDX_ARGS];

		if (args->nodes.GetSize() != 2)
			continue;

		auto arg0 = args->nodes[0]->GetTypeDesc(p);
		auto arg1 = args->nodes[1]->GetTypeDesc(p);

		if (ltype.IsConst() && arg0.IsReference() && !arg0.IsConst())
			continue;

		if (rtype.IsConst() && arg1.IsReference() && !arg1.IsConst())
			continue;

		if (&arg0.GetType() != &ltype.GetType() || &arg1.GetType() != &rtype.GetType())
			continue;

		if (AstStaticCast<const AstText *>(op->nodes[AstFunc::IDX_NAME])->text != opName)
			continue;

		candidates.Add(op);
	}

	// one last thing to solve: if we have more candidates where one can grab by non-const ref and another by const ref, pick the non-const ref
	// FIXME: this is ugly...
	for (Int i=1; i<candidates.GetSize(); i++)
	{
		auto *iargs = candidates[i]->nodes[AstFunc::IDX_ARGS];
		auto iarg0 = iargs->nodes[0]->GetTypeDesc(p);
		auto iarg1 = iargs->nodes[1]->GetTypeDesc(p);

		// skip non-ref overloads
		if (!iarg0.IsReference() && !iarg1.IsReference())
			continue;

		for (Int j=0; j<i; j++)
		{
			auto *jargs = candidates[j]->nodes[AstFunc::IDX_ARGS];
			auto jarg0 = jargs->nodes[0]->GetTypeDesc(p);
			auto jarg1 = jargs->nodes[1]->GetTypeDesc(p);

			// try to match type and ref-type
			if (&iarg0.GetType() != &jarg0.GetType() || &iarg1.GetType() != &jarg1.GetType())
				continue;

			if (iarg0.IsReference() != jarg0.IsReference() || iarg1.IsReference() != jarg1.IsReference())
				continue;

			// keep the one with non-const reference
			auto iconst = iarg0.IsConst() + iarg1.IsConst();
			auto jconst = jarg0.IsConst() + jarg1.IsConst();

			if (iconst > jconst)
				candidates.EraseIndex(i);
			else
				candidates.EraseIndex(j--);

			// erase only once, abort search
			i = candidates.GetSize();
			break;
		}
	}

	if (!candidates.IsEmpty())
		res = candidates[0];

	if (candidates.GetSize() > 1)
	{
		res = nullptr;

		Int idx = 1;

		for (auto *it : candidates)
			p.Error(it, String::Printf("overload candidate #%d", idx++));
	}

	return res;
}

Int NamedScope::AllocVar(const QDataType &ndesc, bool alignStack)
{
	if (!ndesc.GetSize())
		return varOfs;

	Int align = Max<Int>(ndesc.GetAlign(), Stack::WORD_SIZE);
	Int size = Max<Int>(ndesc.GetSize(), Stack::WORD_SIZE);

	if (alignStack)
	{
		size += Stack::WORD_SIZE-1;
		size &= ~((UInt)Stack::WORD_SIZE-1u);
	}

	maxVarAlign = Max(maxVarAlign, align);

	// in 32-bit mode we only align stack up to 4 bytes
	align = Min<Int>(align, Stack::WORD_SIZE);

	Int toAlign = (align - varOfs%align) % align;
	varOfs += toAlign + size;
	varSize += toAlign + size;

	maxVarSize = Max(maxVarSize, varOfs);

	localVars.Add(LocalVariable());
	LocalVariable &lv = localVars.Back();
	lv.offset = varOfs;
	lv.type = ndesc;
	return varOfs;
}

bool NamedScope::IsLocal() const
{
	return type == NSCOPE_FUNCTION || type == NSCOPE_LOCAL || type == NSCOPE_LOOP
		   || type == NSCOPE_ARGS || type == NSCOPE_SWITCH;
}

bool NamedScope::IsGlobal() const
{
	return type == NSCOPE_GLOBAL || type == NSCOPE_NAMESPACE;
}

bool NamedScope::IsComposite() const
{
	return type == NSCOPE_STRUCT || type == NSCOPE_CLASS;
}

bool NamedScope::SetBase(NamedScope *nbase)
{
	LETHE_RET_FALSE(nbase != this);

	base = nbase;
	return true;
}

bool NamedScope::HasDestructors() const
{
	for (Int i = localVars.GetSize() - 1; i >= 0; i--)
	{
		const LocalVariable &lv = localVars[i];
		const QDataType &dt = lv.type;

		if (dt.HasDtor())
			return 1;
	}

	return 0;
}

void NamedScope::GenDestructors(CompiledProgram &p, Int baseLocalVar)
{
	for (Int i=localVars.GetSize()-1; i>=baseLocalVar; i--)
	{
		const LocalVariable &lv = localVars[i];
		const QDataType &dt = lv.type;

		if (dt.IsReference() || !dt.HasDtor() || (dt.qualifiers & AST_Q_SKIP_DTOR))
		{
			// no dtor required
			continue;
		}

		p.EmitLocalDtor(dt.GetType(), varOfs - lv.offset);
	}
}

void NamedScope::AddBreakHandle(Int handle)
{
	breakHandles.Add(handle);
}

void NamedScope::AddContinueHandle(Int handle)
{
	continueHandles.Add(handle);
}

bool NamedScope::HasBreakHandles() const
{
	return !breakHandles.IsEmpty();
}

bool NamedScope::FixupBreakHandles(CompiledProgram &p)
{
	return p.FixupHandles(breakHandles);
}

bool NamedScope::FixupContinueHandles(CompiledProgram &p)
{
	return p.FixupHandles(continueHandles);
}

bool NamedScope::Merge(NamedScope &ns, const Compiler &c, HashMap<NamedScope *, NamedScope *> &scopeRemap)
{
	scopeRemap[&ns] = this;

	LETHE_ASSERT(!ns.base);

	for (auto &it : ns.namedScopes)
	{
		LETHE_ASSERT(!it.value->base);

		auto nsidx = namedScopes.FindIndex(it.key);

		if (nsidx >= 0)
		{
			if (it.value->type != NSCOPE_NAMESPACE)
			{
				LETHE_ASSERT(it.value->node);
				c.onError(String::Printf("Illegal redefinition of `%s'", it.key.Ansi()), it.value->node->location);
				return false;
			}

			// merge recursively!
			LETHE_RET_FALSE(namedScopes.GetValue(nsidx)->Merge(*it.value, c, scopeRemap));
			continue;
		}

		namedScopes[it.key] = it.value;
		LETHE_ASSERT(it.value->parent == &ns);
		it.value->parent = this;
	}

	for (auto &it : ns.scopes)
	{
		LETHE_ASSERT(!it->base);
		it->parent = this;
	}

	scopes.Append(ns.scopes);

	for (auto &it : ns.members)
	{
		auto midx = members.FindIndex(it.key);

		if (midx >= 0)
		{
			c.onError(String::Printf("Illegal redefinition of `%s'", it.key.Ansi()), it.value->location);
			return false;
		}

		members[it.key] = it.value;
	}

	ns.scopes.Clear();
	ns.namedScopes.Clear();
	ns.members.Clear();

	return true;
}


}
