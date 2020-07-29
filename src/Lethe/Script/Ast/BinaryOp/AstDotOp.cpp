#include "AstDotOp.h"
#include "../AstSymbol.h"
#include "../NamedScope.h"
#include "../AstVarDecl.h"
#include <Lethe/Script/Ast/Constants/AstConstInt.h>
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(AstDotOp)

// AstDotOp

bool AstDotOp::FoldConst(const CompiledProgram &p)
{
	auto tn = target;

	LETHE_RET_FALSE(tn);

	if (tn->type == AST_TYPE_ARRAY && nodes[IDX_RIGHT]->type == AST_IDENT)
	{
		if (tn->nodes.GetSize() > 1 && nodes[IDX_RIGHT]->target && nodes[IDX_RIGHT]->target->type == AST_NPROP)
		{
			while (tn->nodes[1]->FoldConst(p));

			auto cnode = tn->nodes[1];

			// this is array size => convert this node to const int!
			if (cnode->type != AST_CONST_INT)
				return false;

			auto dims = AstStaticCast<AstConstInt *>(cnode)->num.i;

			auto nnode = new AstConstInt(location);
			nnode->num.i = dims;

			LETHE_VERIFY(parent->ReplaceChild(this, nnode));
			delete this;
			return true;
		}
	}

	return false;
}

AstNode *AstDotOp::GetResolveTarget() const
{
	return nodes[1]->GetResolveTarget();
}

AstNode *AstDotOp::FindVarSymbolNode()
{
	return nodes[IDX_LEFT]->FindVarSymbolNode();
}

AstNode *AstDotOp::FindSymbolNode(String &sname, const NamedScope *&nscope) const
{
	return nodes[IDX_RIGHT]->FindSymbolNode(sname, nscope);
}

const AstNode *AstDotOp::GetTypeNode() const
{
	return nodes[IDX_RIGHT]->GetTypeNode();
}

bool AstDotOp::ResolveNode(const ErrorHandler &e)
{
	// [IDX_LEFT] = left, [IDX_RIGHT] = right

	// actually we don't go deeper...
	// how to resolve:
	// - we don't know types yet...
	// - but we should be able to resolve stuff...

	// - we seed symScope member var and GetSymScope() which will iterate over parents;
	//   returns scopeRef if none is found

	// problem: left can be scope res or subscript
	// also we need to handle this and super
	// only if it's symbol, we should be fine
	AstNode *left = nodes[IDX_LEFT];

	LETHE_RET_FALSE(left->Resolve(e) != RES_ERROR);

	if (!left->IsResolved())
		return 1;

	AstNode *ltarget = left->GetResolveTarget();

	if (!ltarget)
		return 1;

	const AstNode *tn = ltarget->GetTypeNode();

	if (tn && tn->type == AST_TYPE_AUTO)
	{
		const auto *autotnode = tn->GetTypeNode();
		tn = autotnode == tn ? nullptr : autotnode;
	}

	if (!tn)
		return 1;

	// injecting native props here
	switch(tn->type)
	{
	case AST_TYPE_DYNAMIC_ARRAY:
		symScopeRef = e.dynamicArrayScope;
		break;

	case AST_TYPE_ARRAY_REF:
		symScopeRef = e.arrayRefScope;
		break;

	case AST_TYPE_ARRAY:
		symScopeRef = e.arrayScope;
		break;

	case AST_TYPE_STRING:
		symScopeRef = e.stringScope;
		break;

	default:
		symScopeRef = tn->scopeRef;
	}

	if (!nodes[IDX_RIGHT]->ResolveNode(e))
	{
		e.Error(nodes[IDX_RIGHT], "failed to resolve node");
		return false;
	}

	if (nodes[IDX_RIGHT]->IsResolved())
	{
		flags |= AST_F_RESOLVED;
		target = const_cast<AstNode *>(tn);
	}

	return true;
}

QDataType AstDotOp::GetTypeDesc(const CompiledProgram &p) const
{
	return nodes[IDX_RIGHT]->GetTypeDesc(p);
}

bool AstDotOp::CodeGen(CompiledProgram &p)
{
	AstNode *left = nodes[IDX_LEFT];
	AstNode *right = nodes[IDX_RIGHT];

	// handle virtual props
	if (right->qualifiers & AST_Q_PROPERTY)
	{
		if (right->type != AST_IDENT)
			return p.Error(right, "doesn't resolve to virtual property");

		LockProp();
		auto res = AstStaticCast<AstSymbol *>(right)->CallPropertyGetterViaPtr(p, this);
		UnlockProp();

		return res;
	}

	LETHE_RET_FALSE(left->CodeGenRef(p, 1, 1));
	// ok, we have pointer on top of stack...

	if (right->type == AST_IDENT)
	{
		AstSymbol *text = AstStaticCast<AstSymbol *>(right);
		const NamedScope *nscope = right->target->scopeRef;

		// handle native props
		if (right->target->type == AST_NPROP)
		{
			nscope = right->symScopeRef;
			auto sym = nscope->FindSymbol(text->text);
			LETHE_ASSERT(sym);

			if (sym->offset)
				p.EmitI24(OPC_AADD_ICONST, sym->offset);

			p.PopStackType(1);
			return EmitPtrLoad(QDataType::MakeConstType(p.elemTypes[DT_INT]), p);
		}

		QDataType mdt = nscope->node ? nscope->node->GetTypeDesc(p) : QDataType();
		const DataType::Member *m = mdt.ref->FindMember(text->text);

		if (!m)
			return p.Error(right, "symbol not found in composite type");

		if (m->offset)
			p.EmitI24(OPC_AADD_ICONST, m->offset);

		p.PopStackType(1);
		return EmitPtrLoad(right->GetTypeDesc(p), p);
	}

	return p.Error(this, "unexpected node after `.'");
}

bool AstDotOp::CodeGenRef(CompiledProgram &p, bool allowConst, bool derefPtr)
{
	AstNode *left  = nodes[IDX_LEFT];
	AstNode *right = nodes[IDX_RIGHT];

	LETHE_RET_FALSE(left->CodeGenRef(p, allowConst, 1));
	// ok, we have pointer on top of stack...

	if (right->type == AST_IDENT)
	{
		if (right->qualifiers & AST_Q_PROPERTY)
		{
			if (refPropLock)
				return true;

			return p.Error(right, "cannot generate virtual property reference");
		}

		auto targ = right->target;

		// handle native method props
		if (targ && targ->type == AST_NPROP_METHOD)
			return true;

		// this fixes is() call
		if (targ && (targ->qualifiers & (AST_Q_NATIVE | AST_Q_INTRINSIC | AST_Q_METHOD)) == (AST_Q_NATIVE | AST_Q_INTRINSIC | AST_Q_METHOD))
			return true;

		auto dt = nodes[IDX_RIGHT]->GetTypeDesc(p);
		dt.qualifiers |= AST_Q_REFERENCE;
		p.PopStackType(true);
		p.PushStackType(dt);

		// handle methods here
		if (targ && targ->type == AST_FUNC)
			return true;

		AstSymbol *text = AstStaticCast<AstSymbol *>(right);

		LETHE_ASSERT(right->target);
		const NamedScope *nscope = right->target->scopeRef;

		// handle native props
		if (right->target->type == AST_NPROP)
		{
			if (!allowConst)
				return p.Error(this, "cannot pass as non-const ref");

			nscope = right->symScopeRef;
			auto sym = nscope->FindSymbol(text->text);
			LETHE_ASSERT(sym);

			if (sym->offset)
				p.EmitI24(OPC_AADD_ICONST, sym->offset);

			return true;
		}

		QDataType mdt = nscope->node ? nscope->node->GetTypeDesc(p) : QDataType();
		const DataType::Member *m = mdt.ref->FindMember(text->text);

		if (!m)
			return p.Error(right, "symbol not found in composite type");

		if (m->offset)
			p.EmitI24(OPC_AADD_ICONST, m->offset);

		if (derefPtr && m->node->GetTypeDesc(p).IsPointer())
			p.Emit(OPC_PLOADPTR_IMM);

		if (!allowConst && text->target->type == AST_VAR_DECL)
		{
			auto *vdecl = AstStaticCast<AstVarDecl *>(text->target);
			++vdecl->modifiedCounter;
		}

		return true;
	}

	return p.Error(this, "unexpected node after `.'");
}

bool AstDotOp::TypeGen(CompiledProgram &p)
{
	LETHE_RET_FALSE(Super::TypeGen(p));

	auto *left = nodes[IDX_LEFT];

	if (left->GetTypeDesc(p).GetTypeEnum() == DT_WEAK_PTR)
		return p.Error(this, "cannot dereference weak pointer directly");

	return true;
}

void AstDotOp::LockProp()
{
	++refPropLock;
}

void AstDotOp::UnlockProp()
{
	--refPropLock;
}

}
