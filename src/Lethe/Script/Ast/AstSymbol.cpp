#include "AstSymbol.h"
#include "AstVarDecl.h"
#include "NamedScope.h"
#include "Constants/AstConstant.h"
#include "Constants/AstConstInt.h"
#include "Function/AstFunc.h"
#include "Function/AstCall.h"
#include "Types/AstTypeClass.h"
#include "Types/AstTypeInt.h"
#include "Types/AstTypeVoid.h"
#include "BinaryOp/AstDotOp.h"

#include "Constants/AstConstName.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Stack.h>

#include "CodeGenTables.h"

namespace lethe
{

// AstSymbol

bool AstSymbol::FoldConst(const CompiledProgram &p)
{
	auto *ctarget = DerefConstant(p);
	LETHE_RET_FALSE(ctarget);
	auto dt = GetTypeDesc(p);
	auto *conv = ctarget->ConvertConstNode(ctarget->GetTypeDesc(p).GetType(), dt.GetTypeEnum(), p);
	LETHE_RET_FALSE(conv && conv != ctarget);
	conv->parent = parent;
	LETHE_VERIFY(parent->ReplaceChild(this, conv));
	delete this;
	return true;
}

AstNode *AstSymbol::FindSymbolNode(String &sname, const NamedScope *&nscope) const
{
	sname = text;
	AstNode *res = (symScopeRef ? symScopeRef : scopeRef)->FindSymbolFull(text, nscope);
	return res;
}

AstSymbol *AstSymbol::FindVarSymbolNode(bool)
{
	return this;
}

AstNode *AstSymbol::DerefConstant(const CompiledProgram &p)
{
	LETHE_RET_FALSE(target);

	QDataType dt = target->GetTypeDesc(p);

	if (target->type == AST_ENUM_ITEM && dt.IsConst())
	{
		AstNode *constNode = target->nodes[0];

		if (constNode->IsConstant())
		{
			LETHE_RET_FALSE(constNode->ConvertConstTo(DT_INT, p));
			constNode = target->nodes[0];

			AstStaticCast<AstConstInt *>(constNode)->typeRef = target->GetTypeDesc(p).ref;

			return constNode;
		}
	}

	if (dt.IsConst() && target->type == AST_VAR_DECL && target->nodes.GetSize() > 1)
	{
		AstNode *constNode = target->nodes[1];

		if (constNode->IsConstant())
			return constNode;
	}

	return 0;
}

Int AstSymbol::ToBoolConstant(const CompiledProgram &p)
{
	AstNode *cnode = DerefConstant(p);

	if (!cnode)
		return -1;

	return cnode->ToBoolConstant(p);
}

bool AstSymbol::ResolveNode(const ErrorHandler &e)
{
	if (target)
		return true;

	// look up scope!
	LETHE_ASSERT(scopeRef);
	// we don't want to do full lookup for right nodes of dot op; those will be only looked for in base chain

	bool rightDotNode = parent && parent->type == AST_OP_DOT && parent->nodes[1] == this;

	auto *symScope = GetSymScope(rightDotNode);

	if (!symScope)
		return true;

	auto *sym = symScope->FindSymbolFull(text, symScopeRef, rightDotNode);

	if (sym)
	{
		if (sym && sym->type == AST_VAR_DECL && sym->parent->nodes[0] == this)
			return e.Error(this, "var decl recursion");

		// this is necessary for symbol aliases used by state vars inside state functions
		if (sym && sym->type == AST_TYPEDEF && (sym->qualifiers & AST_Q_SYMBOL_ALIAS))
		{
			sym = sym->nodes[0];

			if (sym)
				sym = sym->target;
		}

		target = sym;
		target->flags |= AST_F_REFERENCED;

		// copy property/bitfield flags from target
		qualifiers |= target->qualifiers & (AST_Q_PROPERTY | AST_Q_BITFIELD);

		flags |= AST_F_RESOLVED;
	}

	return true;
}

const AstNode *AstSymbol::GetTypeNode() const
{
	if (type == AST_NPROP)
	{
		static AstTypeInt tnode{TokenLocation()};
		return &tnode;
	}

	if (target && (target->type == AST_VAR_DECL || target->type == AST_ARG || target->type == AST_NPROP))
		return target->GetTypeNode();

	if (target && target->type == AST_TYPEDEF)
		return target->GetTypeNode();

	return target;
}

QDataType AstSymbol::GetTypeDesc(const CompiledProgram &p) const
{
	if (type == AST_NPROP)
		return QDataType::MakeConstType(p.elemTypes[DT_INT]);

	QDataType res = target ? target->GetTypeDesc(p) : QDataType();

	// the problem here is if target is a typedef we don't convert pointers properly...
	// even worse, typedef may have qualifiers of its own, so...
	// new rules: if weak or raw is specified, it replaces typedef's pointer qualifiers

	const AstNode *vtarget = target;

	if (res.IsPointer() && (qualifiers & (AST_Q_RAW | AST_Q_WEAK)) && target && target->type == AST_TYPEDEF)
	{
		auto *tnode = target->GetTypeNode();
		LETHE_ASSERT(tnode && tnode->type == AST_CLASS);

		// force ptr conversion via DT_CLASS
		if (tnode && tnode->type == AST_CLASS)
		{
			vtarget = tnode;
			res = res.GetType().elemType;
			res.qualifiers &= ~(AST_Q_RAW | AST_Q_WEAK);
		}
	}

	// handle class => ptr conversion here
	if (res.GetTypeEnum() == DT_CLASS)
	{
		res = AstStaticCast<const AstTypeClass *>(vtarget)->GetTypeDescPtr(
			static_cast<DataTypeEnum>(
				((qualifiers & AST_Q_WEAK) != 0) * DT_WEAK_PTR +
				((qualifiers & AST_Q_RAW) != 0) * DT_RAW_PTR
			)
		);
	}

	res.qualifiers |= qualifiers;

	return res;
}

bool AstSymbol::CodeGenRef(CompiledProgram &p, bool allowConst, bool derefPtr)
{
	LETHE_RET_FALSE(Validate(p));

	LETHE_ASSERT(target);

	if (qualifiers & AST_Q_BITFIELD)
		return p.Error(this, "can't generate reference to a bitfield");

	if (qualifiers & AST_Q_PROPERTY)
		return p.Error(this, "can't generate reference to a virtual property");

	const NamedScope *lscopeRef = target->scopeRef;
	QDataType dt = target->GetTypeDesc(p);

	if (!allowConst && dt.IsConst() && !(qualifiers & AST_Q_CAN_MODIFY_CONSTANT))
		return p.Error(this, "cannot modify a constant");

	LETHE_ASSERT(parent);

	// very dumb static analysis that allows us to check trivial cases where a symbol being referred to gets modified and then accessed
	// via a reference
	// typical case: get ref to array elem, then push to array, then access via ref
	if (!allowConst && target->type == AST_VAR_DECL)
	{
		bool isModification = true;

		// FIXME: this may not work in all cases, but we want to eliminate lots of false positives this way
		if (parent->type == AST_OP_SUBSCRIPT)
			isModification = parent->nodes[0] == this && parent->GetTypeDesc(p).GetTypeEnum() == DT_DYNAMIC_ARRAY;

		if (isModification)
			++AstStaticCast<AstVarDecl *>(target)->modifiedCounter;
	}

	Int frameOfs = target->offset;

	if (frameOfs < 0)
		return p.Error(this, String::Printf("variable used before allocated: %s", text.Ansi()));

	bool deref = derefPtr && dt.IsPointer();

	QDataType refType = dt;
	refType.qualifiers |= AST_Q_REFERENCE;

	bool isStatic = (qualifiers & AST_Q_STATIC) != 0;

	if (!isStatic && lscopeRef->IsComposite())
	{
		if (!scopeRef->FindThis())
			return p.Error(this, "this not accessible from here");

		if (!allowConst && scopeRef->IsConstMethod())
			return p.Error(this, "this is const");

		if (dt.IsReference() || deref)
		{
			p.Emit(OPC_PUSHTHIS_TEMP);
			p.EmitI24(OPC_PLOADPTR_IMM, frameOfs);

			if (deref)
			{
				if (dt.IsReference())
					p.Emit(OPC_PLOADPTR_IMM);

				p.PushStackType(dt);
				return true;
			}
		}
		else
		{
			p.Emit(OPC_PUSHTHIS_TEMP);

			if (frameOfs)
				p.EmitI24(OPC_AADD_ICONST, frameOfs);
		}

		p.PushStackType(refType);
		return true;
	}

	if (isStatic || lscopeRef->IsGlobal())
	{
		if (dt.IsReference() || deref)
		{
			p.EmitI24(OPC_GLOADPTR, frameOfs);

			if (deref)
			{
				if (dt.IsReference())
					p.Emit(OPC_PLOADPTR_IMM);

				p.PushStackType(dt);
				return true;
			}
		}
		else
			p.EmitI24(OPC_GLOADADR, frameOfs);

		p.PushStackType(refType);
		return true;
	}

	if (!lscopeRef->IsLocal())
		return p.Error(this, "unsupported symbol scope type");

	// necessary for nested scopes
	lscopeRef = p.curScope;
	Int frameDist = lscopeRef->varSize;
	Int delta = frameDist - frameOfs;
	LETHE_ASSERT(delta >= 0);
	delta += p.exprStackOfs;
	delta /= Stack::WORD_SIZE;

	if (dt.IsReference() || deref)
	{
		p.EmitU24(OPC_LPUSHPTR, delta);

		if (deref)
		{
			if (dt.IsReference())
				p.Emit(OPC_PLOADPTR_IMM);

			p.PushStackType(dt);
			return true;
		}
	}
	else
		p.EmitU24(OPC_LPUSHADR, delta);

	// note: JIT only
	if (target->qualifiers & AST_Q_REF_ALIASED)
		p.FlushOpt(true);

	p.PushStackType(refType);

	return true;
}

bool AstSymbol::TypeGen(CompiledProgram &p)
{
	if (!target)
	{
		auto txt = text;
		AstNode *reportNode = this;

		if (type == AST_OP_SCOPE_RES && txt.IsEmpty() && !nodes.IsEmpty())
		{
			auto *n = nodes.Back();

			if (n->type == AST_IDENT)
			{
				txt = AstStaticCast<AstText *>(n)->text;
				reportNode = n;
			}
		}

		return p.Error(reportNode, String::Printf("symbol not found: `%s'", txt.Ansi()));
	}

	// check accessibility
	auto *thisScope = scopeRef->FindThis(true);
	auto *targThisScope = target->scopeRef->FindThis(true);
	// FIXME: is this ok? can't call GetTypeDesc() here as it might not have been generated yet...
	auto q = target->qualifiers;

	qualifiers |= q & AST_Q_STATIC;

	if (q & (AST_Q_PRIVATE | AST_Q_PROTECTED))
	{
		if (thisScope != targThisScope && !targThisScope->IsParentOf(thisScope) && !thisScope->IsParentOf(targThisScope))
		{
			bool ok = false;

			if (q & AST_Q_PROTECTED)
			{
				// allow one-level nested protected
				if (targThisScope->parent && targThisScope->parent->IsBaseOf(thisScope))
					targThisScope = targThisScope->parent;

				ok = thisScope && targThisScope && targThisScope->IsBaseOf(thisScope);
			}

			if (!ok)
				return p.Error(this, String::Printf("`%s' not accessible", text.Ansi()));
		}
	}

	auto res = Super::TypeGen(p);

	// auto-convert symbol to name if it's an argument to call (for convenience)
	if (res && !(flags & AST_F_SKIP_CGEN) && parent)
	{
		auto *targ = target;

		// follow typedef ... this is stupid (FIXME: should add a method for this)
		while (targ && targ->type == AST_TYPEDEF)
			targ = targ->nodes[0]->target;

		if (targ)
		{
			auto tdesc = targ->GetTypeDesc(p);

			if ((tdesc.GetTypeEnum() == DT_CLASS || targ->type == AST_STRUCT) && (parent->type == AST_CALL || parent->type == AST_OP_EQ || parent->type == AST_OP_NEQ))
			{
				AstNode *newNode = new AstConstName(tdesc.GetType().name, location);
				parent->ReplaceChild(this, newNode);
				delete this;
				return newNode->TypeGen(p);
			}
		}
	}

	// mark funcptr/delegates as referenced but don't mark self as this is a symbol
	if (target && target->type == AST_FUNC && parent && parent->type != AST_FUNC)
		target->qualifiers |= AST_Q_FUNC_REFERENCED;

	return res;
}

bool AstSymbol::Validate(const CompiledProgram &p) const
{
	if (flags & AST_F_SKIP_CGEN)
		return true;

	// make sure we don't access non-static outer member
	if ( !(target->qualifiers & AST_Q_STATIC))
	{
		if (target->type == AST_VAR_DECL && (target->parent->nodes[0]->qualifiers & AST_Q_STATIC))
			return true;

		auto *thisScope = scopeRef->FindThis(true);
		auto *targThisScope = target->scopeRef->FindThis(true);

		if (thisScope && targThisScope && !targThisScope->IsBaseOf(thisScope) && targThisScope->IsParentOf(thisScope))
			return p.Error(this, "foreign member not accessible from here");
	}

	bool ok = true;

	if (target->type == AST_VAR_DECL)
	{
		auto *vdecl = AstStaticCast<AstVarDecl *>(target);

		for (auto &&it : vdecl->liveRefs)
		{
			if (it.var->modifiedCounter == it.counter)
				continue;

			auto *txt = AstStaticCast<AstText *>(it.var->nodes[0]);
			p.Error(this, String::Printf("reference accessed after `%s` was modified", txt->text.Ansi()));
			ok = false;
		}
	}

	return ok;
}

bool AstSymbol::CodeGen(CompiledProgram &p)
{
	LETHE_RET_FALSE(CodeGenInternal(p));

	if (qualifiers & AST_Q_BITFIELD)
		return BitfieldLoad(p, this);

	return true;
}

bool AstSymbol::CodeGenInternal(CompiledProgram &p)
{
	if (flags & AST_F_SKIP_CGEN)
		return true;

	LETHE_RET_FALSE(Validate(p));

	if (qualifiers & AST_Q_PROPERTY)
		return CallPropertyGetterLocal(p);

	p.SetLocation(location);

	LETHE_ASSERT(nodes.IsEmpty() && target);

	const NamedScope *lscopeRef = target->scopeRef;

	if (target->type == AST_FUNC)
	{
		AstFunc *fn = reinterpret_cast<AstFunc *>(target);

		if (fn->qualifiers & AST_Q_INTRINSIC)
			return p.Error(this, "can't create function pointers to intrinsic functions");

		if (fn->qualifiers & AST_Q_METHOD)
		{
			auto *thisScope = scopeRef->FindThis();

			if (!thisScope)
				return p.Error(this, "this not accessible - can't create delegate");

			bool structFlag = thisScope->type != NSCOPE_CLASS;

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
				// we need to validate here
				LETHE_RET_FALSE(ValidateMethod(p, thisScope, fn));

				if (fn->offset >= 0)
					p.EmitBackwardJump(OPC_PUSH_FUNC, fn->offset);
				else
					fn->AddForwardRef(p.EmitForwardJump(OPC_PUSH_FUNC));
			}

			p.Emit(OPC_PUSHTHIS_TEMP);

			if (structFlag)
				p.EmitI24(OPC_BCALL, BUILTIN_MARK_STRUCT_DELEGATE);

			p.PushStackType(fn->GetTypeDesc(p));
			return true;
		}

		if (fn->offset >= 0)
			p.EmitBackwardJump(OPC_PUSH_FUNC, fn->offset);
		else
			fn->AddForwardRef(p.EmitForwardJump(OPC_PUSH_FUNC));

		p.PushStackType(fn->GetTypeDesc(p));
		return true;
	}

	QDataType dt = target->GetTypeDesc(p);

	// try direct const loads if possible
	AstNode *constNode = DerefConstant(p);

	if (constNode)
		return constNode->CodeGen(p);

	Int frameOfs = target->offset;

	if (frameOfs < 0)
		return p.Error(this, String::Printf("variable used before allocated: %s", text.Ansi()));

	QDataType refType = dt;
	refType.qualifiers |= AST_Q_REFERENCE;

	if (dt.GetTypeEnum() == DT_DYNAMIC_ARRAY)
	{
		// load as array ref
		dt.ref = dt.ref->complementaryType;
		dt.qualifiers &= ~AST_Q_DTOR;
	}

	const auto dte = dt.GetTypeEnum();

	LETHE_ASSERT((dt.GetType().type > DT_NONE && dt.GetType().type <= DT_STRING) || dte == DT_DELEGATE ||
				dt.GetType().IsArray() || dt.GetType().IsStruct() || dt.GetType().IsPointer());

	bool isStatic = (qualifiers & AST_Q_STATIC) != 0;

	if (!isStatic && lscopeRef->IsComposite())
	{
		// FIXME: better!
		if (!scopeRef->FindThis())
			return p.Error(this, "this not accessible from here");

		// FIXME: it's getting SO ugly!!! this piece of crap needs refactoring!
		if (dte == DT_ARRAY_REF)
		{
			p.Emit(OPC_PUSHTHIS_TEMP);

			if (frameOfs+sizeof(UIntPtr))
				p.EmitI24(OPC_AADD_ICONST, frameOfs+sizeof(UIntPtr));

			p.Emit(OPC_PLOAD32_IMM);

			p.Emit(OPC_PUSHTHIS_TEMP);

			if (frameOfs)
				p.EmitI24(OPC_AADD_ICONST, frameOfs);

			p.Emit(OPC_PLOADPTR_IMM);
			refType.RemoveReference();
			refType.ref = dt.ref;
		}
		else
		{
			p.Emit(OPC_PUSHTHIS_TEMP);

			if (frameOfs)
				p.EmitI24(OPC_AADD_ICONST, frameOfs);

			if (dt.IsStruct() && dt.IsReference())
			{
				// must dereference member struct
				p.Emit(OPC_PLOADPTR_IMM);
			}
		}

		if (dt.IsArray())
		{
			p.PushStackType(refType);
			return true;
		}

		return EmitPtrLoad(dt, p);
	}

	if (isStatic || lscopeRef->IsGlobal())
	{
		if (dt.IsReference())
			return p.Error(this, "global references not supported");

		// emit_global_stuff!
		// FIXME: better!
		if (dt.IsArray())
		{
			if (dte == DT_ARRAY_REF)
			{
				if (dt.IsReference())
					return p.Error(this, "array refs cannot be passed as references");

				p.EmitU24(OPC_GLOAD32, frameOfs+Stack::WORD_SIZE);
				p.EmitU24(OPC_GLOADPTR, frameOfs);
				p.PushStackType(dt);
				return true;
			}

			p.EmitU24(OPC_GLOADADR, frameOfs);
			p.PushStackType(refType);
			return true;
		}

		if (dt.IsStruct())
		{
			bool hasDtor = dt.HasDtor();
			Int stkSize = (dt.GetSize() + Stack::WORD_SIZE-1)/Stack::WORD_SIZE;
			p.EmitU24Zero(hasDtor ? OPC_PUSHZ_RAW : OPC_PUSH_RAW, stkSize);
			// push source adr
			p.EmitU24(OPC_GLOADADR, frameOfs);
			// push dest adr
			p.EmitI24(OPC_LPUSHADR, 1);

			if (hasDtor)
			{
				p.EmitBackwardJump(OPC_CALL, dt.GetType().funAssign);
				p.EmitI24(OPC_POP, 2);
			}
			else
				p.EmitU24(OPC_PCOPY, dt.GetSize());
		}
		else if (dte == DT_STRING)
		{
			p.EmitIntConst(frameOfs);
			p.Emit(OPC_BCALL + (BUILTIN_GSTRLOAD << 8));
		}
		else if (dte == DT_WEAK_PTR || dte == DT_DELEGATE)
		{
			p.EmitU24(OPC_GLOADADR, frameOfs);
			return EmitPtrLoad(dt, p);
		}
		else
		{
			if (dt.IsMethodPtr())
				return p.Error(this, "cannot load method");

			p.EmitU24(opcodeGlobalLoad[Min<Int>(dt.GetTypeEnum(), DT_FUNC_PTR)], frameOfs);

			if (dte == DT_STRONG_PTR)
				dt.qualifiers |= AST_Q_SKIP_DTOR;
		}

		p.PushStackType(dt);
		return true;
	}

	if (lscopeRef->IsLocal())
	{
		// necessary for nested scopes
		lscopeRef = p.curScope;
	}

	Int frameDist = lscopeRef->varSize;
	Int delta = frameDist - frameOfs;

	if (delta < 0)
		return p.Error(this, "negative stack frame offset");

	delta += p.exprStackOfs;
	delta /= Stack::WORD_SIZE;

	if (dt.IsArray())
	{
		if (dte == DT_ARRAY_REF)
		{
			// note: this is valid if we pass dyn array as arg by ref, then try to copy it to local var!
			if (dt.IsReference())
			{
				p.EmitU24(OPC_LPUSHPTR, delta);
				return EmitPtrLoad(dt, p);
			}

			p.EmitU24(OPC_LPUSH32, delta+1);
			p.EmitU24(OPC_LPUSHPTR, delta+1);
			p.PushStackType(dt);
			return 1;
		}

		p.EmitU24(dt.IsReference() ? OPC_LPUSHPTR : OPC_LPUSHADR, delta);
		p.PushStackType(refType);
		return 1;
	}

	if (dt.IsReference())
	{
		if (dte == DT_NONE || (dte > DT_STRING && !dt.IsStruct() && !dt.IsPointer() && dte != DT_DELEGATE))
			return p.Error(this, "cannot dereference this type");

		p.EmitU24(OPC_LPUSHPTR, delta);
		return EmitPtrLoad(dt, p);
	}

	if (dt.IsStruct())
	{
		bool hasDtor = dt.HasDtor();
		bool isNonTrivial = dt.HasCtor() && (dt.qualifiers & AST_Q_NONTRIVIAL);

		Int stkSize = (dt.GetSize() + Stack::WORD_SIZE-1)/Stack::WORD_SIZE;
		p.EmitU24Zero(hasDtor ? OPC_PUSHZ_RAW : OPC_PUSH_RAW, stkSize);

		// we need to call ctor for non-trivial structs
		if (isNonTrivial)
		{
			p.EmitI24(OPC_LPUSHADR, 0);
			p.EmitBackwardJump(OPC_CALL, dt.GetType().funCtor);
			p.EmitI24(OPC_POP, 1);
		}

		// push source adr
		p.EmitU24(OPC_LPUSHADR, stkSize + delta);
		// push dest adr
		p.EmitI24(OPC_LPUSHADR, 1);

		if (hasDtor)
		{
			p.EmitBackwardJump(OPC_CALL, dt.GetType().funAssign);
			p.EmitI24(OPC_POP, 2);
		}
		else
			p.EmitU24(OPC_PCOPY, dt.GetSize());
	}
	else if (dte == DT_STRING)
	{
		p.EmitI24(OPC_PUSH_ICONST,  delta);
		p.Emit(OPC_BCALL | (BUILTIN_LPUSHSTR << 8));
	}
	else if (dte == DT_WEAK_PTR || dte == DT_DELEGATE)
	{
		p.EmitU24(OPC_LPUSHADR, delta);
		return EmitPtrLoad(dt, p);
	}
	else
	{
		if (dt.IsMethodPtr())
			return p.Error(this, "cannot load method");

		p.EmitU24(opcodeLocalLoad[Min<Int>(dt.GetTypeEnum(), DT_FUNC_PTR)], delta);

		if (dte == DT_STRONG_PTR)
			dt.qualifiers |= AST_Q_SKIP_DTOR;
	}

	// not: JIT only
	if (target->qualifiers & AST_Q_REF_ALIASED)
	{
		// FIXME: this hack forces to spill anything at and above delta, but it's tricky without global analysis
		p.EmitU24(OPC_LPUSHADR, delta);
		p.EmitI24(OPC_POP, 1);
	}

	p.PushStackType(dt);
	return true;
}

bool AstSymbol::CallPropertyGetterLocal(CompiledProgram &p)
{
	return CallPropertyGetterViaPtr(p, this);
}

bool AstSymbol::CallPropertyGetterViaPtr(CompiledProgram &p, AstNode *root)
{
	LETHE_ASSERT(symScopeRef);

	AstNode *ftarget;

	// inside block to save stack space
	{
		StringBuilder sb;
		sb += "__get_";
		sb += text;
		ftarget = symScopeRef->FindSymbol(sb.Get());
	}

	if (!ftarget || ftarget->type != AST_FUNC)
		return p.Error(ftarget ? ftarget : this, "property getter not found");

	auto retType = AstStaticCast<AstFunc *>(ftarget)->nodes[AstFunc::IDX_RET]->GetTypeDesc(p);
	auto wantType = GetTypeDesc(p);

	p.SetLocation(location);

	// synthesize call
	AstCall tcall(location);

	tcall.nodes.Add(root);
	tcall.scopeRef = root->scopeRef;
	tcall.forceFunc = ftarget;
	tcall.parent = root->parent;

	bool res = tcall.CodeGen(p);

	tcall.nodes.Clear();

	if (res)
		res = p.EmitConv(this, retType, wantType);

	return res;
}

bool AstSymbol::CallPropertySetter(CompiledProgram &p, AstNode *dnode, AstNode *snode)
{
	auto *symNode = dnode;

	if (dnode->type == AST_OP_DOT)
		symNode = dnode->nodes[1];

	if (symNode->type != AST_IDENT)
		return p.Error(symNode, "not a symbol");

	auto *sym = AstStaticCast<AstSymbol *>(symNode);

	AstNode *ftarget;

	// inside block to save stack space
	{
		StringBuilder sb;
		sb += "__set_";
		sb += sym->text;
		ftarget = sym->symScopeRef->FindSymbol(sb.Get());
	}

	if (!ftarget || ftarget->type != AST_FUNC)
		return p.Error(ftarget ? ftarget : sym, "property setter not found");

	// synthesize call
	AstCall tcall(sym->location);

	auto *dot = AstStaticCast<AstDotOp *>(dnode->type == AST_OP_DOT ? dnode : nullptr);

	if (dot)
		dot->LockProp();

	tcall.nodes.Add(dnode);
	tcall.nodes.Add(snode);
	tcall.scopeRef = sym->scopeRef;
	tcall.forceFunc = ftarget;
	tcall.parent = dnode->parent;

	bool res = tcall.CodeGen(p);

	if (dot)
		dot->UnlockProp();

	tcall.nodes.Clear();

	return res;
}

bool AstSymbol::ValidateBitfield(CompiledProgram &p, AstNode *node, const QDataType &tpe)
{
	if (!tpe.IsNumber())
		return p.Error(node, "bitfield must be a number");

	if (tpe.IsFloatingPoint())
		return p.Error(node, "bitfield must be an integer");

	if (tpe.GetTypeEnum() == DT_ENUM)
		return p.Error(node, "bitfield cannot be an enum");

	if (tpe.IsLongInt())
		return p.Error(node, "bitfields not supported for long integers");

	LETHE_ASSERT(node->target);

	return true;
}

bool AstSymbol::BitfieldStore(CompiledProgram &p, AstNode *node, AstNode *dnode, AstNode *snode)
{
	auto qdt = node->GetTypeDesc(p);
	LETHE_RET_FALSE(ValidateBitfield(p, node, qdt));

	if (node->type != AST_IDENT)
		return p.Error(node, "expected identifier");

	auto *genNode = dnode;

	bool useThis = false;

	if (dnode->type == AST_OP_DOT)
		genNode = dnode->nodes[0];
	else
	{
		useThis = true;

		if (!dnode->symScopeRef)
			return p.Error(dnode, "no symbol scope");

		genNode = dnode->symScopeRef->node;
	}

	auto sdt = genNode->GetTypeDesc(p);

	if (sdt.IsPointer())
		sdt = sdt.GetType().elemType;

	auto *m = sdt.GetType().FindMember(AstStaticCast<AstText *>(node)->text);

	if (!m)
		return p.Error(node, "member not found");

	if (useThis)
	{
		if (!dnode->scopeRef->FindThis())
			return p.Error(dnode, "this not found");

		p.Emit(OPC_PUSHTHIS_TEMP);
		p.PushStackType(QDataType::MakeConstType(p.elemTypes[DT_STRONG_PTR]));
	}
	else
	{
		LETHE_RET_FALSE(genNode->CodeGenRef(p, false, true));
	}
	// ok, we got ptr to struct/class start on stack

	p.EmitIntConst(m->offset);
	p.EmitU24(OPC_AADD, 1);

	Instruction opLoad = OPC_HALT, opStore = OPC_HALT;

	switch(qdt.GetSize())
	{
	case 1:
		opLoad = OPC_PLOAD8U_IMM;
		opStore = OPC_PSTORE8_IMM;
		break;
	case 2:
		opLoad = OPC_PLOAD16U_IMM;
		opStore = OPC_PSTORE16_IMM;
		break;
	case 4:
		opLoad = OPC_PLOAD32_IMM;
		opStore = OPC_PSTORE32_IMM;
		break;
	}

	p.Emit(OPC_LPUSHPTR);
	p.Emit(opLoad);

	auto bitSize = (node->target->num.i & 65535);

	LETHE_ASSERT(node->target->type == AST_VAR_DECL);

	// look up type in var decl list
	auto num = node->target->parent->nodes[0]->num.i;

	auto bitShift = (num >> 16);

	// mask
	Int mask = Int(((ULong)1 << bitSize)-1);
	UInt invMask = mask;
	invMask <<= bitShift;
	invMask = ~invMask;

	p.EmitIntConst((Int)invMask);
	p.Emit(OPC_IAND);

	// we have an extra item on stack so adjust before calling CodeGen
	p.PushStackType(QDataType::MakeConstType(p.elemTypes[DT_INT]));

	LETHE_RET_FALSE(snode->CodeGen(p));
	p.PopStackType(true);
	p.PopStackType(true);
	p.PopStackType(true);

	if (p.CanOptPrevious() && (p.instructions.Back() & 255) == OPC_PUSH_ICONST)
	{
		// optimize constant case
		auto iconst = Int(p.instructions.Back()) >> 8;
		p.instructions.Pop();
		iconst &= mask;
		iconst <<= bitShift;

		if (iconst != 0)
		{
			p.EmitIntConst(iconst);
			p.Emit(OPC_IOR);
		}
	}
	else
	{
		p.EmitIntConst(mask);
		p.Emit(OPC_IAND);

		if (bitShift > 0)
			p.EmitI24(OPC_ISHL_ICONST, bitShift);

		p.Emit(OPC_IOR);
	}

	p.EmitI24(OPC_LPUSHPTR, 1);
	p.Emit(opStore);

	p.EmitI24(OPC_POP, 1);

	return true;
}

bool AstSymbol::BitfieldLoad(CompiledProgram &p, AstNode *node)
{
	auto qdt = node->GetTypeDesc(p);
	LETHE_RET_FALSE(ValidateBitfield(p, node, qdt));

	auto bitSize = (node->target->num.i & 65535);

	LETHE_ASSERT(node->target->type == AST_VAR_DECL);

	// look up type in var decl list
	auto num = node->target->parent->nodes[0]->num.i;

	auto bitShift = (num >> 16);

	auto isSigned = qdt.IsSigned();

	if (bitSize <= 0 || bitSize > 32)
		return p.Error(node, "invalid bitfield size");

	if (isSigned)
	{
		Int shl = 32 - bitSize - bitShift;

		if (shl > 0)
		{
			p.EmitI24(OPC_ISHL_ICONST, shl);
		}

		Int sar = 32 - bitSize;

		if (sar > 0)
			p.EmitI24(OPC_ISAR_ICONST, sar);
	}
	else
	{
		Int mask = Int(((ULong)1 << bitSize)-1);

		if (bitShift > 0)
			p.EmitI24(OPC_ISHR_ICONST, bitShift);

		p.EmitIntConst(mask);
		p.Emit(OPC_IAND);
	}

	return true;
}


}
