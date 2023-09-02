#include "AstDotOp.h"
#include "../AstSymbol.h"
#include "../NamedScope.h"
#include "../AstVarDecl.h"
#include <Lethe/Script/Ast/Constants/AstConstInt.h>
#include <Lethe/Script/Ast/Function/AstFunc.h>
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

#include <Lethe/Script/Compiler/Warnings.h>

namespace lethe
{

LETHE_AST_BUCKET_ALLOC_DEF(AstDotOp)

// AstDotOp

bool AstDotOp::FoldConst(const CompiledProgram &p)
{
	if (Super::FoldConst(p))
		return true;

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

AstSymbol *AstDotOp::FindVarSymbolNode(bool preferLocal)
{
	return nodes[IDX_LEFT]->FindVarSymbolNode(preferLocal);
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
	LETHE_RET_FALSE(CodeGenInternal(p));

	auto *right = nodes[IDX_RIGHT];

	// handle bitfields
	if (right->qualifiers & AST_Q_BITFIELD)
		return AstSymbol::BitfieldLoad(p, right);

	return true;
}

bool AstDotOp::CodeGenInternal(CompiledProgram &p)
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

	const bool isFuncLoad = right->type == AST_IDENT && right->target->type == AST_FUNC;

	if (isFuncLoad)
	{
		// we'll be loading a funcptr or delegate
		auto *fn = AstStaticCast<AstFunc *>(right->target);

		if (fn->qualifiers & AST_Q_STATIC)
		{
				if (fn->offset >= 0)
					p.EmitBackwardJump(OPC_PUSH_FUNC, fn->offset);
				else
					fn->AddForwardRef(p.EmitForwardJump(OPC_PUSH_FUNC));

				p.PushStackType(fn->GetTypeDesc(p));
				return true;
		}

		// check if struct
		auto *fscope = fn->scopeRef->FindThis();
		bool structFlag = fscope && fscope->type != NSCOPE_CLASS;

		if (fn->vtblIndex >= 0 && !(qualifiers & AST_Q_NON_VIRT))
		{
			// new delegates: if LSBit is 1, funptr is actually vtbl index*4 (=dynamic vtbl binding)
			// bit 1 marks a struct
			// this new way is necessary to work as expected with dynamic vtable changes
			p.EmitI24(OPC_PUSHZ_RAW, 1);
			p.EmitIntConst(fn->vtblIndex*4 + 1);
			p.EmitI24(OPC_LSTORE32, 1);
			// we now have vfunc ptr on stack
		}
		else
		{
			if (fn->offset >= 0)
				p.EmitBackwardJump(OPC_PUSH_FUNC, fn->offset);
			else
				fn->AddForwardRef(p.EmitForwardJump(OPC_PUSH_FUNC));
		}

		p.PushStackType(QDataType::MakeConstType(p.elemTypes[DT_FUNC_PTR]));

		LETHE_RET_FALSE(left->CodeGenRef(p, 1, 1));
		p.PopStackType(1);
		p.PopStackType(1);

		if (structFlag)
			p.EmitI24(OPC_BCALL, BUILTIN_MARK_STRUCT_DELEGATE);

		p.PushStackType(fn->GetTypeDesc(p));
		return true;
	}

	LETHE_RET_FALSE(left->CodeGenRef(p, 1, 1));
	// ok, we have pointer on top of stack...

	p.SetLocation(right->location);

	if (right->type == AST_IDENT)
	{
		AstSymbol *text = AstStaticCast<AstSymbol *>(right);
		const NamedScope *nscope = right->target->scopeRef;

		// handle native props
		if (right->target->type == AST_NPROP)
		{
			if (parent && parent->type == AST_EXPR)
				p.Warning(this, "discarding result of a native property", WARN_DISCARD);

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

		// dereference here
		if (m->type.IsReference())
			p.Emit(OPC_PLOADPTR_IMM);

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

	p.SetLocation(right->location);

	if (right->type == AST_IDENT)
	{
		if (right->qualifiers & AST_Q_BITFIELD)
			return p.Error(right, "cannot generate reference to a bitfield");

		if (right->qualifiers & AST_Q_PROPERTY)
		{
			if (refPropLock)
				return true;

			return p.Error(right, "cannot generate reference to a virtual property");
		}

		auto *targ = right->target;

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
		LETHE_RET_FALSE(right->target);
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

		if (m->type.IsReference())
			p.Emit(OPC_PLOADPTR_IMM);

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
