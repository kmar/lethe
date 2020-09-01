#include "AstTypeAuto.h"
#include "AstTypeClass.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include "../NamedScope.h"
#include "../AstSymbol.h"

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(AstTypeAuto)

// AstTypeAuto

AstNode::ResolveResult AstTypeAuto::Resolve(const ErrorHandler &e)
{
	bool resolved = (flags & AST_F_RESOLVED) != 0;

	auto res = Super::Resolve(e);

	auto *at = GetAutoType();

	if (at && ((at->flags & AST_F_RESOLVED) || (at->type == AST_STRUCT || at->type == AST_CLASS)))
	{
		flags |= AST_F_RESOLVED;
		target = at;
	}

	if (res == RES_ERROR)
		return res;

	if (!resolved && (flags & AST_F_RESOLVED))
		res = RES_MORE;

	return res;
}

bool AstTypeAuto::TypeGen(CompiledProgram &)
{
	if (!typeCache)
		typeCache = GetAutoType();

	return true;
}

const AstNode *AstTypeAuto::GetTypeNode() const
{
	// avoid problems like auto a=a
	if (++typeLockCount >= 100)
		return nullptr;

	const auto *atype = GetAutoType();
	LETHE_RET_FALSE(atype);
	const AstNode *res = atype->GetTypeNode();

	--typeLockCount;

	return res ? res : this;
}

AstNode *AstTypeAuto::GetAutoType() const
{
	auto *res = GetExprNode();

	if (res)
	{
		auto *nres = const_cast<AstNode *>(res->GetTypeNode());

		if (nres)
			res = nres;

		if (res->type == AST_ENUM_ITEM)
		{
			// we have to switch to underlying type!
			LETHE_ASSERT(res->parent && res->parent->type == AST_ENUM);
			res = res->parent;
			typeCache = res;
		}
	}

	return res;
}

AstNode *AstTypeAuto::GetExprNode() const
{
	AstNode *vdecl;
	AstNode *res = nullptr;

	switch (parent->type)
	{
	case AST_VAR_DECL_LIST:
		if (qualifiers & AST_Q_AUTO_RANGE_FOR)
		{
			auto *fornode = parent->parent;
			auto *cmpnode = fornode->nodes[1];

			if (cmpnode->type == AST_OP_LT)
			{
				res = cmpnode->nodes[1];
				break;
			}
		}

		vdecl = parent->nodes[1];

		if (vdecl->nodes.GetSize() >= 2)
			res = vdecl->nodes[1];

		break;

	case AST_ARG:
		vdecl = parent;

		if (vdecl->nodes.GetSize() >= 3)
			res = vdecl->nodes[2];

		break;
	default:;
	}

	return res;
}

QDataType AstTypeAuto::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;

	if (lockFlag)
	{
		if (!reportedFlag)
		{
			reportedFlag = true;
			p.Error(this, "recursive auto declaration");
		}
		return QDataType();
	}

	lockFlag = true;

	const auto *tnode = typeCache ? typeCache : GetAutoType();

	if (tnode)
	{
		res = tnode->GetTypeDesc(p);

		LETHE_ASSERT(parent);

		auto *at = (qualifiers & AST_Q_REFERENCE) ? GetExprNode() : nullptr;

		// for auto-ref, copy const qualifier
		res.qualifiers |= at ? at->GetTypeDesc(p).qualifiers & AST_Q_CONST : 0;

		if (at && !(res.qualifiers & AST_Q_STATIC) && parent->scopeRef->IsConstMethod())
		{
			auto *sym = at->FindVarSymbolNode();

			if (sym && sym->symScopeRef && sym->symScopeRef->IsBaseOf(parent->scopeRef->FindThis()))
			{
				// force const for refs to members inside const methods
				res.qualifiers |= AST_Q_CONST;
			}
		}

		if (res.GetTypeEnum() == DT_CLASS)
			res = AstStaticCast<const AstTypeClass *>(tnode)->GetTypeDescPtr(DT_STRONG_PTR);

		res.RemoveReference();

		if (res.IsPointer())
		{
			bool wantWeak = (qualifiers & AST_Q_WEAK) != 0;
			bool wantRaw = (qualifiers & AST_Q_RAW) != 0;

			res.ref = res.ref->GetPointerType(wantRaw ? DT_RAW_PTR : wantWeak ? DT_WEAK_PTR : DT_STRONG_PTR);
			res.qualifiers |= AST_Q_SKIP_DTOR;
		}

		res.qualifiers |= qualifiers;
	}

	lockFlag = false;

	return res;
}

void AstTypeAuto::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstTypeAuto *>(n);
	tmp->typeCache = typeCache;
	tmp->lockFlag = lockFlag;
	tmp->reportedFlag = reportedFlag;
	tmp->typeLockCount = typeLockCount;
}


}
