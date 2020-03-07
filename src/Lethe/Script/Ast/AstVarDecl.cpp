#include "AstVarDecl.h"
#include "AstText.h"
#include "AstSymbol.h"
#include "NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Stack.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/Ast/Function/AstFunc.h>
#include <Lethe/Script/Compiler/Warnings.h>
#include "CodeGenTables.h"

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(AstVarDecl)

// AstVarDecl

QDataType AstVarDecl::GetTypeDesc(const CompiledProgram &p) const
{
	return parent->nodes[0]->GetTypeDesc(p);
}

const AstNode *AstVarDecl::GetTypeNode() const
{
	const AstNode *ntype = parent->nodes[0];

	const AstNode *res = ntype->GetTypeNode();

	return res ? res : ntype->target;
}

AstNode *AstVarDecl::GetResolveTarget() const
{
	auto ntype = parent->nodes[0];
	return ntype->GetResolveTarget();
}

bool AstVarDecl::CodeGenComposite(CompiledProgram &p)
{
	LETHE_ASSERT(scopeRef && parent);
	LETHE_ASSERT(parent->type == AST_VAR_DECL_LIST);

	auto varType = parent->nodes[0];
	auto tdesc = varType->GetTypeDesc(p);

	bool isInitializerList = nodes.GetSize() > 1 && nodes[1]->type == AST_INITIALIZER_LIST;
	bool isStatic = (varType->qualifiers & AST_Q_STATIC) != 0;

	if (isInitializerList)
	{
		// const + POD const init => make static to save space
		// this cannot be here as it breaks nrvo!
		if ((varType->qualifiers & AST_Q_CONST) && !isStatic && scopeRef->IsLocal() && nodes[1]->IsInitializerConst(p, tdesc))
		{
			qualifiers |= AST_Q_STATIC;

			// move to global scope (FIXME: hacky)
			while (scopeRef && !scopeRef->IsGlobal())
				scopeRef = scopeRef->parent;

			LETHE_ASSERT(scopeRef);
		}
	}

	return Super::CodeGenComposite(p);
}

bool AstVarDecl::CallInit(CompiledProgram &p, const AstNode *varType, Int globalOfs, Int localOfs)
{
	// try to call __init
	LETHE_ASSERT(varType && varType->target);
	auto sym = varType->target->scopeRef->FindSymbol(p.GetInternalFuncName(CompiledProgram::IFUNC_INIT), true);

	if (sym && sym->type == AST_FUNC)
	{
		if (sym->qualifiers & (AST_Q_STATIC | AST_Q_NATIVE))
			return p.Error(sym, "__init must be nonstatic, non-native member function");

		auto fn = AstStaticCast<AstFunc *>(sym);

		if (!fn->GetArgs()->nodes.IsEmpty())
			return p.Error(sym, "__init must have 0 arguments");

		if (fn->nodes[0]->GetTypeDesc(p).GetTypeEnum() != DT_NONE)
			return p.Error(sym, "__init must return void");

		if (globalOfs >= 0)
			p.EmitI24(OPC_GLOADADR, globalOfs);
		else
			p.EmitI24(OPC_LPUSHADR, localOfs);

		p.Emit(OPC_LOADTHIS);

		if (sym->offset >= 0)
			p.EmitBackwardJump(OPC_CALL, sym->offset);
		else
			fn->AddForwardRef(p.EmitForwardJump(OPC_CALL));

		p.Emit(OPC_POPTHIS);
	}

	return true;
}

void AstVarDecl::AddLiveRefs(AstNode *n)
{
	if (n->type != AST_OP_SUBSCRIPT && n->type != AST_IDENT)
	{
		for (auto *it : n->nodes)
			AddLiveRefs(it);

		return;
	}

	if (n->type == AST_OP_SUBSCRIPT)
		n = n->nodes[0];

	while (n)
	{
		if (n->type == AST_IDENT)
		{
			auto *lsym = AstStaticCast<AstSymbol *>(n);

			if (lsym->target->type == AST_VAR_DECL)
			{
				auto *vdecl = AstStaticCast<AstVarDecl *>(lsym->target);
				liveRefs.Add(LiveRef{vdecl, vdecl->modifiedCounter});
			}
		}
		else if (n->type == AST_OP_DOT || n->type == AST_OP_SCOPE_RES)
		{
			if (n->type == AST_OP_DOT)
				AddLiveRefs(n->nodes[0]);

			n = n->nodes[1];
			continue;
		}
		else if (n->type == AST_OP_SUBSCRIPT)
		{
			n = n->nodes[0];
			continue;
		}

		break;
	}
}

bool AstVarDecl::CodeGen(CompiledProgram &p)
{
	// FIXME: refactor, this code is super-messy with copy-pasted code!!!
	// should have similar logic for all scopes

	// must know if it's local, global or member
	// at the moment, only local are supported

	AstNode *varType = parent->nodes[0];

	QDataType tdesc = varType->GetTypeDesc(p);
	tdesc.qualifiers &= ~AST_Q_SKIP_DTOR;

	if (tdesc.qualifiers & AST_Q_CONSTEXPR)
		return true;

	if ((tdesc.qualifiers & AST_Q_NOCOPY) && nodes.GetSize() > 1)
		return p.Error(this, "cannot initialize nocopy variable");

	bool isInit = nodes.GetSize() > 1;
	bool isInitializerList = isInit && nodes[1]->type == AST_INITIALIZER_LIST;
	bool isStatic = ((varType->qualifiers & AST_Q_STATIC) != 0) || ((qualifiers & AST_Q_STATIC) != 0);
	bool isCompleteInitializerList = false;

	if (tdesc.GetTypeEnum() == DT_STATIC_ARRAY && isInit && !isInitializerList)
		return p.Error(this, "static arrays must be initialized via initializer list");

	if (isInit && tdesc.IsReference() && tdesc.IsPointer() && !tdesc.IsConst())
	{
		auto idt = nodes[1]->GetTypeDesc(p);

		if (idt.IsPointer() && (idt.qualifiers & AST_Q_NOCOPY))
			return p.Error(nodes[1], "cannot create lvalue ref to nocopy arg pointer");
	}

	if (isInitializerList)
	{
		// check if fully initialized
		isCompleteInitializerList = nodes[1]->IsCompleteInitializerList(p, tdesc);
	}

	if (!isStatic && scopeRef->IsLocal())
	{
		// local

		if (nodes[0]->type == AST_IDENT && !(flags & AST_F_REFERENCED) && !(varType->qualifiers & AST_Q_CONST)
				&& !(tdesc.qualifiers & (AST_Q_CTOR | AST_Q_DTOR)))
		{
			if (!tdesc.IsSmartPointer() || nodes.GetSize() <= 1)
			{
				p.Warning(this, String::Printf("unreferenced local variable: %s",
					AstStaticCast<const AstText *>(nodes[0])->text.Ansi()), WARN_UNREFERENCED);
				// don't report more
				flags |= AST_F_REFERENCED;
			}
		}

		// got local scope => ok
		// now get type alignment and size
		// TODO: implement!
		if (tdesc.GetSize() <= 0 && tdesc.GetTypeEnum() != DT_STRUCT)
			return p.Error(varType, "invalid type size (overflow)");

		if (tdesc.GetSize() > 512 * 1024)
			return p.Error(this, "variable too big to fit on stack (>512kb)");

		// we have two cases now: with init or without init
		// FIXME: refactor!
		if (!isInitializerList && nodes.GetSize() > 1)
		{
			if (tdesc.IsReference() && tdesc.GetType() != nodes[1]->GetTypeDesc(p).GetType())
				return p.Error(this, "incompatible types");

			LETHE_RET_FALSE(nodes[1]->ConvertConstTo(tdesc.GetTypeEnum(), p));

			if (p.GetJitFriendly() && ((tdesc.IsReference() && !tdesc.IsConst()) || tdesc.HasArrayRefWithNonConstElem()))
			{
				// we want to solve a problem for JIT; find leftmost symbol AND mark it as REF_ALIASED
				// later when generating code this should force a reload via pointer using lpushadr/ptrload
				// we don't want to do this for the interpreter however!
				// another thing which remains is array ref filled from local array
				// even if we can't catch every corner case, this should get rid of "surprises"
				auto *snode = nodes[1]->FindVarSymbolNode();

				if (snode && snode->target && snode->scopeRef && snode->scopeRef->IsLocal())
					snode->target->qualifiers |= AST_Q_REF_ALIASED;
			}

			LETHE_RET_FALSE(tdesc.IsReference() ? nodes[1]->CodeGenRef(p, tdesc.IsConst()) : nodes[1]->CodeGen(p));

			if (tdesc.IsReference())
			{
				// rebuild liveRefs list
				liveRefs.Clear();

				AddLiveRefs(nodes[1]);
			}

			// pop and store... must know current stack state... sigh
			// also current design won't allow to generate assembly code... (really? what about using this as intermediate representation?)
			// with init => now store result; but this means we have to know what's on stack
			// (and types must match or at least be convertible => semantic check here)

			if (p.exprStack.GetSize() < 1)
				return p.Error(this, "initializer expression must return a value");

			auto top = p.exprStack.Back();

			// perform conversion if necessary
			if (tdesc.GetType() != top.GetType())
			{
				LETHE_RET_FALSE(p.EmitConv(nodes[1], top, tdesc.GetType()));
				top = p.exprStack.Back();
			}

			if (!top.IsReference() && (top.qualifiers & AST_Q_SKIP_DTOR) && top.IsPointer())
			{
				// this is to fix a case when we do A a = this;
				p.EmitAddRef(top);
			}

			if (!tdesc.CanAlias(top))
				return p.Error(this, "incompatible types");

			p.DynArrayVarFix(tdesc);

			p.PopStackType(1);
		}
		else if (varType->qualifiers & AST_Q_REFERENCE)
			return p.Error(this, "local references must be initialized");

		Int oldOfs = scopeRef->varOfs;
		Int varIdx = ((flags & AST_F_NRVO) && offset >= 0) ? offset : scopeRef->AllocVar(tdesc);
		offset = varIdx;

		if (isInitializerList || nodes.GetSize() <= 1)
		{
			Int words = (scopeRef->varOfs - oldOfs + Stack::WORD_SIZE-1)/Stack::WORD_SIZE;

			if (words)
			{
				// always zero-init; we could only skip zero-init if:
				// - no ctor/dtor needed and noinit specified
				// - full initializer specified

				bool noinit = (tdesc.qualifiers & AST_Q_NOINIT) != 0 || isCompleteInitializerList;
				bool zeroInit = tdesc.ZeroInit() || !noinit;

				p.EmitU24(zeroInit ? OPC_PUSHZ_RAW : OPC_PUSH_NOZERO, words);
			}

			if (!(flags & AST_F_NRVO))
				p.EmitCtor(tdesc);
		}

		if (isInitializerList)
		{
			auto odelta = p.initializerDelta;
			p.initializerDelta = scopeRef->varOfs - offset;
			LETHE_RET_FALSE(nodes[1]->GenInitializerList(p, tdesc, 0, 0));
			p.initializerDelta = odelta;
		}

		if (isInit && tdesc.IsStruct() && varType->target)
			LETHE_RET_FALSE(CallInit(p, varType, -1));

		// entering variable lifetime (debug info)
		p.StartLocalVarLifeTime(tdesc, offset, AstStaticCast<const AstText *>(nodes[0])->text);

		return true;
	}

	if (scopeRef->IsGlobal() || isStatic)
	{
		scopeRef->blockThis += isStatic;

		if (nodes[0]->type == AST_IDENT && !(flags & AST_F_REFERENCED) && !(varType->qualifiers & AST_Q_CONST))
		{
			p.Warning(this, String::Printf("unreferenced global variable: %s",
										   AstStaticCast<const AstText *>(nodes[0])->text.Ansi()), WARN_UNREFERENCED);
		}

		// we can't possibly guard array references to be safe anyway, so we allow this
		//if (tdesc.HasArrayRef())
		//	return p.Error(varType, "global array references not allowed");

		if (tdesc.IsReference())
			return p.Error(varType, "global references not allowed");

		if (tdesc.GetSize() <= 0 && tdesc.GetTypeEnum() != DT_STRUCT)
			return p.Error(varType, "invalid type size (overflow)");

		if ((Long)p.cpool.data.GetSize() + tdesc.GetSize() > 256*1024*1024)
			return p.Error(varType, "too many globals (256M limit reached)");

		if (nodes[0]->type == AST_IDENT)
		{
			auto *gname = AstStaticCast<const AstText *>(nodes[0]);

			offset = p.cpool.AllocGlobalVar(tdesc, gname->GetQText(p));
		}
		else
			offset = p.cpool.AllocGlobal(tdesc);

		if (!isInitializerList && nodes.GetSize() > 1)
		{
			LETHE_RET_FALSE(nodes[1]->ConvertConstTo(tdesc.GetTypeEnum(), p));

			if (nodes[1]->IsConstant())
			{
				scopeRef->blockThis -= isStatic;

				if (tdesc.HasDtor())
					p.EmitGlobalDtor(this, tdesc.GetType(), offset);

				return BakeGlobalData(nodes[1], tdesc, offset, p);
			}

			LETHE_RET_FALSE(tdesc.IsReference() ? nodes[1]->CodeGenRef(p, tdesc.IsConst()) : nodes[1]->CodeGen(p));

			if (p.exprStack.GetSize() < 1)
				return p.Error(this, "initializer expression must return a value");

			// when doing stuff like int x = func() // returning ref
			if (!tdesc.IsReference() && nodes[1]->GetTypeDesc(p).IsReference())
			{
				p.PopStackType(1);
				LETHE_RET_FALSE(EmitPtrLoad(nodes[1]->GetTypeDesc(p), p));
			}

			auto top = p.exprStack.Back();

			// perform conversion if necessary
			if (tdesc.GetType() != top.GetType())
			{
				LETHE_RET_FALSE(p.EmitConv(nodes[1], top, tdesc.GetType()));
				top = p.exprStack.Back();
			}

			if (!tdesc.CanAlias(top))
				return p.Error(this, "incompatible types");

			p.DynArrayVarFix(tdesc);

			p.PopStackType(1);
			// emit global copy
			LETHE_ASSERT(!tdesc.IsReference());
			LETHE_RET_FALSE(p.EmitGlobalCopy(this, tdesc.GetType(), offset));
		}
		else if (varType->qualifiers & AST_Q_REFERENCE)
			return p.Error(this, "global references must be initialized");

		if (tdesc.HasDtor())
			p.EmitGlobalDtor(this, tdesc.GetType(), offset);

		p.EmitGlobalCtor(tdesc, offset);

		if (isInitializerList)
			LETHE_RET_FALSE(nodes[1]->GenInitializerList(p, tdesc, offset, 1));

		scopeRef->blockThis -= isStatic;

		if (isInit && tdesc.IsStruct() && varType->target)
			LETHE_RET_FALSE(CallInit(p, varType, offset));

		return true;
	}

	if (scopeRef->IsComposite())
	{
		// note: later I'll have to generate code anyway for ctor/dtor...
		return true;
	}

	return false;
}


}
