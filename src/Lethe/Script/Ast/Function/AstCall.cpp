#include "AstCall.h"
#include "AstFunc.h"
#include "../UnaryOp/AstUnaryRef.h"
#include "../AstText.h"
#include "../NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Stack.h>
#include <Lethe/Script/Ast/CodeGenTables.h>
#include <Lethe/Script/Ast/AstText.h>
#include <Lethe/Script/Ast/Types/AstFuncBase.h>
#include <Lethe/Script/Ast/Types/AstTypeDynamicArray.h>
#include <Lethe/Script/Ast/Constants/AstConstName.h>
#include <Lethe/Script/Ast/Constants/AstConstString.h>
#include <Lethe/Script/Ast/Types/AstTypeClass.h>
#include <Lethe/Script/Ast/AstText.h>
#include <Lethe/Script/Utils/FormatStr.h>
#include <Lethe/Core/String/StringBuilder.h>

#include <Lethe/Script/Compiler/Warnings.h>

namespace lethe
{

LETHE_AST_BUCKET_ALLOC_DEF(AstCall)

// AstCall

NamedScope *AstCall::FindSpecialADLScope(const NamedScope *tempScope, const StringRef &nname) const
{
	while (tempScope)
	{
		auto it = tempScope->namedScopes.Find(nname);

		if (it != tempScope->namedScopes.End())
			return it->value.Get();

		tempScope = tempScope->parent ? tempScope->parent : nullptr;
	}

	return nullptr;
}

AstNode::ResolveResult AstCall::Resolve(const ErrorHandler &e)
{
	AstNode::ResolveResult res;

	if (!(nodes[0]->flags & AST_F_RESOLVED) && !(nodes[0]->qualifiers & AST_Q_NON_VIRT) && (nodes[0]->type == AST_IDENT || nodes[0]->type == AST_OP_SCOPE_RES) && nodes.GetSize() > 1)
	{
		// I need to hack in ADL here somehow

		res = Super::ResolveFrom(e, 1);

		if (res != RES_OK)
			return res;

		bool argsResolved = true;

		for (Int i=1; i<nodes.GetSize(); i++)
		{
			if (!nodes[i]->IsResolved())
			{
				argsResolved = false;
				break;
			}
		}

		AstNode *adlNode = nullptr;

		if (argsResolved)
		{
			auto *sym = AstStaticCast<AstText *>(nodes[0]);
			const auto &fname = sym->text;

			// we ultimately don't support ADL with scope res op, I had an idea to combine this with special scopes
			// but that was broken anyway, so let's just not allow it at all
			bool tryADL = e.tryADL && sym->type != AST_OP_SCOPE_RES;

			// here we should be able to do ADL to find the function based on ADL
			for (Int i=1; tryADL && i<nodes.GetSize(); i++)
			{
				if (nodes[i]->type == AST_ARG_ELLIPSIS)
					continue;

				auto *tnode = nodes[i]->GetTypeNode();

				if (!tnode)
					continue;

				auto *tscope = tnode ? tnode->scopeRef : nullptr;

				// virtually inject some scopes for elementary types now
				StringRef specialScopeName;

				switch(tnode->type)
				{
				case AST_TYPE_INT:
					specialScopeName = "__int";
					break;
				case AST_TYPE_UINT:
					specialScopeName = "__uint";
					break;
				case AST_TYPE_LONG:
					specialScopeName = "__long";
					break;
				case AST_TYPE_ULONG:
					specialScopeName = "__ulong";
					break;
				case AST_TYPE_FLOAT:
					specialScopeName = "__float";
					break;
				case AST_TYPE_DOUBLE:
					specialScopeName = "__double";
					break;
				case AST_TYPE_NAME:
					specialScopeName = "__name";
					break;
				case AST_TYPE_STRING:
					specialScopeName = "__string";
					break;
				default:;
				}

				const auto *lookupScope = scopeRef;

				NamedScope *specialScope = specialScopeName.IsEmpty() ? nullptr : FindSpecialADLScope(lookupScope, specialScopeName);

				if (!tscope && specialScope)
					tscope = specialScope;

				if (!tscope)
					continue;

				// now try to look up fname
				auto *funcNode = tscope->FindSymbol(fname, true, true);

				if (!funcNode && specialScope)
				{
					tscope = specialScope;
					funcNode = tscope->FindSymbol(fname, true, true);
				}

				if (funcNode && funcNode->type != AST_NPROP_METHOD && !(funcNode->qualifiers & AST_Q_METHOD))
				{
					if (funcNode->type == AST_FUNC && !AstStaticCast<AstFunc *>(funcNode)->ValidateADLCall(*this, e))
						continue;

					if (adlNode && adlNode != funcNode)
					{
						e.Error(this, String::Printf("ambiguous call to `%s'", fname.Ansi()));
						return RES_ERROR;
					}

					nodes[0]->target = funcNode;
					nodes[0]->symScopeRef = tscope;
					nodes[0]->flags = AST_F_RESOLVED;
					adlNode = funcNode;
				}
			}

			if (!adlNode)
				res = Super::Resolve(e);
		}
	}
	else
		res = Super::Resolve(e);

	if (!nodes[0]->IsResolved())
		return res;

	auto *targ = nodes[0]->GetResolveTarget();

	if (targ)
	{
		// FIXME: special hack to force ADL resolve... damn!
		// (only checks potential recursion)
		if (targ->type == AST_FUNC)
		{
			auto *fscope = scopeRef->FindFunctionScope();

			if (fscope && fscope->node == targ && (!fscope->FindThis() || targ->qualifiers & AST_Q_STATIC) && !(AstStaticCast<AstFunc *>(targ)->ValidateADLCall(*this, e)))
			{
				flags &= ~AST_F_RESOLVED;
				nodes[0]->flags &= ~AST_F_RESOLVED;
				return RES_OK;
			}
		}

		if (targ->type == AST_STRUCT)
		{
			e.Error(this, "invalid call target (struct)");
			return RES_ERROR;
		}

		if (targ->type == AST_CLASS)
		{
			if (nodes.GetSize() == 1 && parent->type == AST_NEW)
			{
				// disguised new class()
				auto tnode = nodes[0];
				nodes.Clear();
				LETHE_VERIFY(parent->ReplaceChild(this, tnode));
				e.AddLateDeleteNode(this);
				return res;
			}

			e.Error(this, "invalid call target (class)");
			return RES_ERROR;
		}
	}

	Int callArgs = nodes.GetSize()-1;

	if (callArgs)
		return res;

	auto replaceProp = [this, &e]()
	{
		// replace with direct prop access!
		parent->ReplaceChild(this, nodes[0]);
		nodes[0] = nullptr;
		e.AddLateDeleteNode(this);
	};

	// try to remap property call to getter
	if (nodes[0]->type == AST_OP_DOT && nodes[0]->nodes[1]->type == AST_IDENT)
	{
		auto *propn = AstStaticCast<AstText *>(nodes[0]->nodes[1]);

		if (propn->symScopeRef && (propn->qualifiers & AST_Q_PROPERTY))
		{
			StringBuilder sb;
			sb += "__get_";
			sb += propn->text;
			auto *refn = propn->symScopeRef->FindSymbol(sb.Get());

			if (refn)
			{
				refn->qualifiers |= AST_Q_FUNC_REFERENCED;
				replaceProp();
				return res;
			}
		}
	}

	// ditto for native props
	auto *rtarg = nodes[0]->GetResolveTarget();

	if (rtarg && rtarg->type == AST_FUNC)
		rtarg->qualifiers |= AST_Q_FUNC_REFERENCED;

	if (!rtarg || rtarg->type != AST_NPROP)
		return res;

	replaceProp();

	return res;
}

const AstNode *AstCall::GetTypeNodeFunc() const
{
	const auto *targ = GetResolveTarget();

	LETHE_RET_FALSE(targ);

	if (targ->type == AST_NPROP_METHOD)
	{
			// got native property
			auto fnameNode = AstStaticCast<const AstText *>(targ);
			const auto &fnameText = fnameNode->text;

			auto *rootScope = scopeRef;

			while (rootScope && rootScope->parent)
				rootScope = rootScope->parent;

			LETHE_RET_FALSE(rootScope);

			auto midx = rootScope->members.FindIndex(fnameText);

			LETHE_RET_FALSE(midx >= 0);

			targ = rootScope->members.GetValue(midx);
			LETHE_ASSERT(targ);
	}

	return targ;
}

const AstNode *AstCall::GetTypeNode() const
{
	const auto *targ = GetTypeNodeFunc();

	LETHE_RET_FALSE(targ);

	bool isFunc = targ->type == AST_FUNC;

	if (isFunc)
		targ = const_cast<AstNode *>(targ->nodes[0]->GetTypeNode());
	else
		targ = const_cast<AstNode *>(targ->GetTypeNode());

	// this condition prevents auto x = function_returning_delegate!
	if (!isFunc && targ && (targ->type == AST_TYPE_DELEGATE || targ->type == AST_TYPE_FUNC_PTR))
		targ = targ->nodes[0]->GetTypeNode();

	return targ;
}

const AstNode *AstCall::GetContextTypeNode(const AstNode *node) const
{
	// 1+ = args
	// 0 = func to call
	auto *fn = GetTypeNodeFunc();
	LETHE_RET_FALSE(fn);

	// FIXME: this const_cast is stupid
	auto idx = nodes.FindIndex(const_cast<AstNode *>(node))-1;
	LETHE_RET_FALSE(idx >= 0);

	AstNode *args = nullptr;

	if (fn->type == AST_FUNC)
	{
		args = fn->nodes[AstFunc::IDX_ARGS];
	}
	else
	{
		fn = fn->GetTypeNode();

		if (fn->type == AST_TYPE_DELEGATE || fn->type == AST_TYPE_FUNC_PTR)
			args = fn->nodes[1];
	}

	LETHE_RET_FALSE(args && idx < args->nodes.GetSize());
	auto *arg = args->nodes[idx];
	LETHE_RET_FALSE(!arg->nodes.IsEmpty());
	return arg->nodes[0]->GetTypeNode();
}

QDataType AstCall::GetTypeDesc(const CompiledProgram &p) const
{
	String fname;
	auto *fdef = FindFunction(fname);
	AstNode *npdef = nullptr;

	if (fdef)
	{
		if (fdef->type == AST_NPROP_METHOD)
		{
			npdef = fdef;
			// got native property... => try to translate fdef
			auto fnameNode = AstStaticCast<AstText *>(fdef);
			const auto &fnameText = fnameNode->text;
			const auto fci = p.nativeMap.Find(fnameText);

			if (fci != p.nativeMap.End())
				fdef = fci->value;
		}

		if (fdef->type == AST_VAR_DECL)
		{
			// must be func ptr
			auto *tnode = fdef->GetTypeNode();

			if (tnode && (tnode->type == AST_TYPE_FUNC_PTR || tnode->type == AST_TYPE_DELEGATE))
				return tnode->nodes[0]->GetTypeDesc(p);
		}

		// must handle typedefs here and funcptrs in args
		if (fdef->type == AST_IDENT || fdef->type == AST_ARG)
		{
			auto qdt = fdef->GetTypeDesc(p);

			if (qdt.GetTypeEnum() == DT_FUNC_PTR || qdt.GetTypeEnum() == DT_DELEGATE)
				return qdt.GetType().elemType;
		}
	}

	if (!fdef || fdef->type != AST_FUNC)
		return Super::GetTypeDesc(p);

	if (npdef)
	{
		LETHE_ASSERT(nodes[0]->type == AST_OP_DOT);

		auto npflags = npdef->flags;

		auto arrType = nodes[0]->nodes[0]->GetTypeDesc(p);

		if (npflags & AST_F_RES_ELEM)
		{
			LETHE_ASSERT(arrType.IsArray());
			return arrType.ref->elemType;
		}

		if (npflags & AST_F_RES_SLICE)
		{
			LETHE_ASSERT(arrType.IsArray());
			auto res = arrType;

			if (res.GetTypeEnum() == DT_DYNAMIC_ARRAY)
				res.ref = res.ref->complementaryType;

			res.RemoveReference();

			return res;
		}
	}

	return fdef->nodes[0]->GetTypeDesc(p);
}

AstNode *AstCall::GetResolveTarget() const
{
	auto targ = nodes[0]->GetResolveTarget();

	if (targ && targ->type == AST_NPROP_METHOD)
	{
		LETHE_ASSERT(nodes[0]->type == AST_OP_DOT);

		auto varDeclType = nodes[0]->nodes[0]->GetResolveTarget();
		auto arrayType = varDeclType ? varDeclType->GetTypeNode() : nullptr;

		AstNodeType arrType = AST_NONE;

		if (arrayType)
			arrType = arrayType->type;

		if (targ->flags & AST_F_RES_ELEM)
		{
			switch (arrType)
			{
			case AST_TYPE_ARRAY:
			case AST_TYPE_ARRAY_REF:
			case AST_TYPE_DYNAMIC_ARRAY:
				return arrayType->nodes[0]->GetResolveTarget();

			default:;
			}
		}

		if (targ->flags & AST_F_RES_SLICE)
		{
			switch (arrType)
			{
			case AST_TYPE_ARRAY_REF:
				return const_cast<AstNode *>(arrayType);
			case AST_TYPE_DYNAMIC_ARRAY:
			{
				auto *aref = AstStaticCast<AstTypeDynamicArray *>(const_cast<AstNode *>(arrayType));
				return aref->GetArrayRefNode();
			}

			default:;
			}
		}
	}

	return targ;
}

bool AstCall::TypeGen(CompiledProgram &p)
{
	// handle ref ellipsis
	auto *fntarg = nodes[0]->target;

	if (fntarg && fntarg->type == AST_FUNC)
	{
		auto *fn = AstStaticCast<AstFuncBase *>(nodes[0]->target);
		auto *args = fn->GetArgs();

		// check if ref ellipsis
		if (args && !args->nodes.IsEmpty() && args->nodes.Back()->type == AST_ARG_ELLIPSIS &&
			(args->nodes.Back()->qualifiers & AST_Q_REFERENCE))
		{
			// first ellipsis argument
			Int firstEllipsis = args->nodes.GetSize();

			// rewrite args
			for (Int i=firstEllipsis; i<nodes.GetSize(); i++)
			{
				auto *n = nodes[i];
				auto *nn = new AstUnaryRef(n->location);
				nn->flags |= AST_F_RESOLVED;

				ReplaceChild(n, nn);

				n->parent = nullptr;
				nn->Add(n);
			}
		}
	}

	// propagate thread unsafe qualifier
	if (fntarg && (fntarg->qualifiers & AST_Q_THREAD_UNSAFE))
		if (auto *fnode = FindEnclosingFunction())
			fnode->qualifiers |= AST_Q_THREAD_UNSAFE;

	return Super::TypeGen(p);
}

AstNode *AstCall::FindFunction(String &fname) const
{
	const NamedScope *tmp;
	return nodes[0]->FindSymbolNode(fname, tmp);
}

const AstFuncBase *AstCall::GetFuncBase() const
{
	String fname;

	const AstFuncBase *res = nullptr;

	auto *fdef = forceFunc ? forceFunc : FindFunction(fname);

	if (!fdef)
		return res;

	if (fdef->type != AST_TYPE_FUNC_PTR)
	{
		if (fdef->type != AST_FUNC && fdef->type != AST_VAR_DECL)
			return res;

		if (fdef->type == AST_VAR_DECL)
			fdef = fdef->parent->nodes[0];
	}

	res = AstStaticCast<AstFuncBase *>(fdef);

	return res;
}

bool AstCall::CodeGenRef(CompiledProgram &p, bool allowConst, bool derefPtr)
{
	// only allow if returns (non)-const reference

	String fname;

	auto *fdef = forceFunc ? forceFunc : FindFunction(fname);

	if (!fdef)
		return p.Error(this, String::Printf("function not found:%s", nodes[0]->GetTextRepresentation().Ansi()));

	// FIXME: avoid copy-paste
	if (fdef->type != AST_TYPE_FUNC_PTR)
	{
		if (fdef->type != AST_FUNC && fdef->type != AST_VAR_DECL)
			return p.Error(this, "invalid type for reference to call");

		QDataType qdt = fdef->GetTypeDesc(p);
		const auto dte = qdt.GetTypeEnum();

		if (dte != DT_FUNC_PTR && dte != DT_DELEGATE)
			return p.Error(this, "can only call a function");

		if (fdef->type == AST_VAR_DECL)
			fdef = fdef->parent->nodes[0];
	}

	auto fn = AstStaticCast<AstFuncBase *>(fdef);
	QDataType dt = fn->GetResult()->GetTypeDesc(p);

	// we can't really load a pointer here because we wouldn't be able to clean up properly ... damnit!
	// would have to allocate virtual local variable for the result (unless it's raw ptr)
	if (!dt.IsReference() && (dt.GetTypeEnum() != DT_RAW_PTR || !derefPtr))
		return p.Error(this, "not an lvalue (reference expected)");

	if (!allowConst && dt.IsConst())
		return p.Error(this, "cannot convert from non-const ref");

	return CodeGenCommon(p, true, derefPtr);
}

bool AstCall::CodeGen(CompiledProgram &p)
{
	// keep refs if parent is expression or comma operator!
	bool keepRef = ShouldPop();
	return CodeGenCommon(p, keepRef, false);
}

bool AstCall::CodeGenIntrinsic(CompiledProgram &p, AstNode *fdef)
{
	AstFuncBase *fn = AstStaticCast<AstFuncBase *>(fdef);

	if (fn->type != AST_FUNC || !(fdef->qualifiers & (AST_Q_NATIVE | AST_Q_METHOD)))
		return p.Error(fdef, "intrinsics must be native functions");

	AstFunc *fun = AstStaticCast<AstFunc *>(fn);
	auto name = AstStaticCast<AstText *>(fun->nodes[AstFunc::IDX_NAME])->GetQText(p);

	// FIXME: better!
	if (name == "sqrt" || name == "__float::sqrt")
	{
		if (nodes.GetSize() != 2)
			return p.Error(this, "invalid number of arguments to sqrt");

		if (!nodes[1]->ConvertConstTo(DT_FLOAT, p))
			return p.Error(nodes[1], "cannot convert constant");

		LETHE_RET_FALSE(nodes[1]->CodeGen(p));

		if (p.exprStack.IsEmpty())
			return p.Error(nodes[1], "argument expression must return a value");

		LETHE_RET_FALSE(p.EmitConv(nodes[1], p.exprStack.Back(), QDataType::MakeConstType(p.elemTypes[DT_FLOAT])));
		p.Emit(OPC_FSQRT);
		p.PopStackType(1);
		p.PushStackType(QDataType::MakeConstType(p.elemTypes[DT_FLOAT]));
		return true;
	}

	if (name == "dsqrt" || name == "__double::sqrt")
	{
		if (nodes.GetSize() != 2)
			return p.Error(this, "invalid number of arguments to dsqrt");

		if (!nodes[1]->ConvertConstTo(DT_DOUBLE, p))
			return p.Error(nodes[1], "cannot convert constant");

		LETHE_RET_FALSE(nodes[1]->CodeGen(p));

		if (p.exprStack.IsEmpty())
			return p.Error(nodes[1], "argument expression must return a value");

		LETHE_RET_FALSE(p.EmitConv(nodes[1], p.exprStack.Back(), QDataType::MakeConstType(p.elemTypes[DT_DOUBLE])));
		p.Emit(OPC_DSQRT);
		p.PopStackType(1);
		p.PushStackType(QDataType::MakeConstType(p.elemTypes[DT_DOUBLE]));
		return true;
	}

	if (name == "bsf" || name == "__uint::bsf" || name == "bsr" || name == "__uint::bsr" || name == "popcnt" || name == "__uint::popcnt" ||
		name == "bswap" || name == "__uint::bswap")
	{
		if (nodes.GetSize() != 2)
			return p.Error(this, String::Printf("invalid number of arguments to %s", name.Ansi()));

		if (!nodes[1]->ConvertConstTo(DT_UINT, p))
			return p.Error(nodes[1], "cannot convert constant");

		LETHE_RET_FALSE(nodes[1]->CodeGen(p));

		if (p.exprStack.IsEmpty())
			return p.Error(nodes[1], "argument expression must return a value");

		LETHE_RET_FALSE(p.EmitConv(nodes[1], p.exprStack.Back(), QDataType::MakeConstType(p.elemTypes[DT_UINT])));

		const bool isBswap = name == "bswap" || name == "__uint::bswap";

		p.Emit(
			name == "bsf" || name == "__uint::bsf" ? (Int)OPC_INTRINSIC_BSF :
			name == "bsr" || name == "__uint::bsr" ? (Int)OPC_INTRINSIC_BSR :
			name == "popcnt" || name == "__uint::popcnt" ? (Int)OPC_INTRINSIC_POPCNT :
			isBswap ? (Int)OPC_INTRINSIC_BSWAP :
			(Int)OPC_HALT
		);

		p.PopStackType(1);
		p.PushStackType(QDataType::MakeConstType(p.elemTypes[isBswap ? DT_UINT : DT_INT]));
		return true;
	}

	if (name == "bsfl" || name == "__ulong::bsf" || name == "bsrl" || name == "__ulong::bsr" || name == "popcntl" || name == "__ulong::popcnt" ||
		name == "bswapl" || name == "__ulong::bswap")
	{
		if (nodes.GetSize() != 2)
			return p.Error(this, String::Printf("invalid number of arguments to %s", name.Ansi()));

		if (!nodes[1]->ConvertConstTo(DT_ULONG, p))
			return p.Error(nodes[1], "cannot convert constant");

		LETHE_RET_FALSE(nodes[1]->CodeGen(p));

		if (p.exprStack.IsEmpty())
			return p.Error(nodes[1], "argument expression must return a value");

		LETHE_RET_FALSE(p.EmitConv(nodes[1], p.exprStack.Back(), QDataType::MakeConstType(p.elemTypes[DT_ULONG])));

		const bool isBswap = name == "bswapl" || name == "__ulong::bswap";

		p.Emit(
			name == "bsfl" || name == "__ulong::bsf" ? (Int)OPC_INTRINSIC_BSFL :
			name == "bsrl" || name == "__ulong::bsr" ? (Int)OPC_INTRINSIC_BSRL :
			name == "popcntl" || name == "__ulong::popcnt" ? (Int)OPC_INTRINSIC_POPCNTL :
			isBswap ? (Int)OPC_INTRINSIC_BSWAPL :
			(Int)OPC_HALT
		);

		p.PopStackType(1);
		p.PushStackType(QDataType::MakeConstType(p.elemTypes[isBswap ? DT_ULONG : DT_INT]));
		return true;
	}

	if (name == "object::IsA" || name == "object::is")
	{
		if (nodes.GetSize() != 2)
			return p.Error(this, "invalid number of arguments to IsA");

		LETHE_RET_FALSE(nodes[0]->CodeGenRef(p, true, true));

		if (p.exprStack.IsEmpty())
			return p.Error(nodes[0], "argument expression must return a value");

		if (!nodes[1]->ConvertConstTo(DT_NAME, p))
			return p.Error(nodes[1], "cannot convert constant");

		LETHE_RET_FALSE(nodes[1]->CodeGen(p));

		if (p.exprStack.IsEmpty())
			return p.Error(nodes[1], "argument expression must return a value");

		LETHE_RET_FALSE(p.EmitConv(nodes[1], p.exprStack.Back(), QDataType::MakeConstType(p.elemTypes[DT_NAME])));

		p.EmitI24(OPC_BCALL, BUILTIN_ISA);
		p.PopStackType(true);
		p.PopStackType(true);
		p.PushStackType(QDataType::MakeType(p.elemTypes[DT_BOOL]));
		return true;
	}

	if (name == "unused")
	{
		// here we simply "voidcast" everything
		for (Int i=1; i<nodes.GetSize(); i++)
		{
			if (nodes[i]->HasSideEffects())
			{
				auto mark = p.ExprStackMark();
				LETHE_RET_FALSE(nodes[i]->CodeGen(p));
				p.ExprStackCleanupTo(mark);
			}
		}

		return true;
	}

	return p.Error(fdef, String::Printf("intrinsic function not found: %s", name.Ansi()));
}

Int AstCall::GenTempCopy(CompiledProgram &p, Int resultBaseOffset, Int resultWords, bool resultZeroed, Int actual, const Array<TempArg> &tempArgs)
{
	resultBaseOffset /= Stack::WORD_SIZE;
	actual /= Stack::WORD_SIZE;

	// copy chunks interleaving with references...
	Int sourceOfs = actual - resultBaseOffset - resultWords;

	Int nextTempArg = 0;

	if (resultWords > 0)
		p.EmitI24(resultZeroed ? OPC_PUSHZ_RAW : OPC_PUSH_RAW, resultWords);

	Int res = resultWords;

	Int src = 0;
	Int dst = res;
	Int count = sourceOfs;
	Int toCopy;

	auto copy = [&]()
	{
		if (toCopy <= 0)
			return;

		auto srcAdr = sourceOfs+res-src-toCopy;
		auto dstAdr = 1+res-dst-toCopy;

		if (toCopy == 1)
		{
			p.EmitI24(OPC_LPUSHPTR, srcAdr);
		}
		else
		{
			p.EmitI24(OPC_PUSH_RAW, toCopy);
			srcAdr += toCopy;
			dstAdr += toCopy;
			// load src adr
			p.EmitI24(OPC_LPUSHADR, srcAdr);
			// load dst adr
			p.EmitI24(OPC_LPUSHADR, dstAdr);
			// copy
			p.EmitU24(OPC_PCOPY, toCopy * Stack::WORD_SIZE);
		}

		src += toCopy;
		dst += toCopy;
		res += toCopy;
	};

	while (src < count)
	{
		toCopy = count - src;

		if (nextTempArg >= tempArgs.GetSize())
		{
			copy();
			continue;
		}

		const auto &targ = tempArgs[nextTempArg++];
		toCopy = targ.offset / Stack::WORD_SIZE - resultBaseOffset - src - resultWords;

		copy();

		auto tsize = targ.size / Stack::WORD_SIZE;

		// and push ref
		p.EmitI24(OPC_LPUSHADR, sourceOfs+res-src-tsize);
		++res;

		src += tsize;
		++dst;
	}

	return res;
}

const AstNode *AstCall::FindEnclosingFunction() const
{
	const AstNode *res = this;

	while (res && res->type != AST_FUNC)
		res = res->parent;

	return res && res->type == AST_FUNC ? res : nullptr;
}

void AstCall::CheckDeprecatedCall(CompiledProgram &p, AstNode *fdef, const Attributes *attrs)
{
	if (!(fdef->qualifiers & AST_Q_DEPRECATED))
		return;

	auto txt = AstStaticCast<AstText *>(fdef->nodes[AstFunc::IDX_NAME])->text;
	p.Warning(this, String::Printf("attempt to call deprecated function `%s'", txt.Ansi()), WARN_DEPRECATED);

	if (!attrs)
		return;

	const auto &tokens = attrs->tokens;

	for (Int i=0; i<tokens.GetSize(); i++)
	{
		const auto &tok = tokens[i];

		if (tok.type != TOK_IDENT || StringRef(tok.text) != "deprecated")
			continue;

		String suffix;

		if (i+2 < tokens.GetSize() && tokens[i+1].type == TOK_LBR && tokens[i+2].type == TOK_STRING)
			p.Warning(this, String::Printf("%s", tokens[i+2].text), WARN_DEPRECATED);

		return;
	}
}

bool AstCall::CodeGenCommon(CompiledProgram &p, bool keepRef, bool derefPtr)
{
	p.SetLocation(location);

	/*
		problem to solve: we should ideally have retval as first arg pushed (assuming args pushed in reverse order)
		BUT this means no ellipsis! OR with ellipsis but without retval (yes, this seems fine since ellipsis is only useful for printf)
		OTOH, if I wouldn't allow ellipsis, functions could clean up themselves, reducing code footprint; actually I can do this automatically
		if there's no ellipsis in decl
		only solution is to pass a reference to retval as first arg... (this is what my C++ compiler does)
		SO, I've a dilemma, passing result as first on stack is very tempting, otherwise I'd have to use register return values
		=> solved, will push retval first
	*/

	// [0] = (specialized) name
	// [1]+ = args

	LETHE_ASSERT(scopeRef);

	String fname;
	auto *fdef = forceFunc ? forceFunc : FindFunction(fname);

	if (!fdef)
		return p.Error(this, "function to call not found!");

	const auto *attrs = fdef->type == AST_FUNC ? AstStaticCast<AstFunc *>(fdef)->attributes.Get() : nullptr;

	CheckDeprecatedCall(p, fdef, attrs);

	if ((fdef->qualifiers & AST_Q_NATIVE) && forceFunc && fname.IsEmpty())
	{
		// get better operator name for native binding
		// TODO: cache full names with types for operators?
		StringBuilder opname = "operator";
		opname += AstStaticCast<const AstText *>(fdef->nodes[1])->text;
		opname += " ";
		opname += forceFunc->GetTypeDesc(p).GetName();
		fname = opname.Get();
	}

	if (fdef->type == AST_FUNC && (fdef->flags & AST_F_SKIP_CGEN) && !(fdef->qualifiers & AST_Q_FUNC_REFERENCED))
		return p.Error(this, "attempting to call a removed function!");

	bool isStateBreak = fdef && (fdef->qualifiers & AST_Q_STATEBREAK) != 0;
	const bool isLatent = fdef && (fdef->qualifiers & AST_Q_LATENT) != 0;
	bool isMethod = fdef && (fdef->qualifiers & AST_Q_METHOD) != 0;

	// we want to allow this as a jump to chain if in state method and if calling non-virtually
	bool isStateFuncCall = fdef && (fdef->qualifiers & AST_Q_STATE) != 0;

	if (isStateFuncCall)
	{
		// only allow from within a state function
		auto *enclosing = FindEnclosingFunction();

		if (!enclosing || !(enclosing->qualifiers & AST_Q_STATE))
			return p.Error(this, "state functions cannot be called directly");

		const auto *thisScope = fdef->scopeRef->FindThis();

		if (!thisScope || !thisScope->node)
			return p.Error(this, "this not found");

		auto *thisNode = AstStaticCast<AstTypeClass *>(thisScope->node);
		auto clsname = thisNode->GetName();

		if (fdef->type != AST_FUNC)
			return p.Error(this, "invalid state function call");

		auto calleeFuncName = AstStaticCast<AstText *>(fdef->nodes[AstFunc::IDX_NAME])->GetQText(p);

		Int istr = p.cpool.Add(calleeFuncName);

		// emit class name, label
		p.EmitULongConst(clsname.GetValue());
		p.EmitIntConst(istr);
		p.EmitI24(OPC_BCALL, BUILTIN_LPUSHSTR_CONST);
		// note: set state label cleans up stack
		p.EmitI24(OPC_BMCALL, BUILTIN_SET_STATE_LABEL);

		// poor man's unreachable code test => nothing is executed explicit state function call
		auto cidx = cachedIndex;
		auto *blk = parent;

		if (blk->type == AST_EXPR)
		{
			cidx = blk->cachedIndex;
			blk = blk->parent;
		}

		if (blk && (blk->type == AST_BLOCK || blk->type == AST_FUNC_BODY) && blk->nodes.GetSize() > cidx+1)
			p.Warning(blk->nodes[cidx+1], "unreachable code");
	}

	if (isLatent || isStateBreak)
	{
		// emit virtual function with label
		auto *curfunc = FindEnclosingFunction();

		if (!curfunc)
			return p.Error(this, "enclosing function not found");

		if (isLatent && !(curfunc->qualifiers & AST_Q_STATE))
			return p.Error(this, "latent function can only be called from state functions");

		if (isStateBreak)
		{
			if (!(curfunc->qualifiers & AST_Q_STATE))
				isStateBreak = false;
		}
		else
		{
			auto insideFunc = AstStaticCast<AstText *>(curfunc->nodes[AstFunc::IDX_NAME])->GetQText(p);

			StringBuilder sb;
			auto latentCounter = p.IncLatentCounter();

			if (latentCounter < 0)
				return p.Error(this, "cannot call latent function here with locals on stack");

			sb.AppendFormat("%s#%d", insideFunc.Ansi(), latentCounter);

			String labelName = sb.Get();
			Int istr = p.cpool.Add(labelName);
			labelName = p.cpool.sPool[istr];

			const auto *thisScope = p.curScope->FindThis();

			if (!thisScope || !thisScope->node)
				return p.Error(this, "this not found");

			auto *thisNode = AstStaticCast<AstTypeClass *>(thisScope->node);
			auto clsname = thisNode->GetName();

			auto *dt = const_cast<DataType *>(p.FindClass(clsname));

			if (!dt)
			{
				LETHE_RET_FALSE(thisNode->TypeGen(p));
				dt = const_cast<DataType *>(p.FindClass(clsname));

				if (!dt)
					return p.Error(this, "class not found");
			}

			// emit class name, label
			p.EmitULongConst(clsname.GetValue());
			p.EmitIntConst(istr);
			p.EmitI24(OPC_BCALL, BUILTIN_LPUSHSTR_CONST);
			// note: set state label cleans up stack
			p.EmitI24(OPC_BMCALL, BUILTIN_SET_STATE_LABEL);

			p.EmitFunc(labelName, nullptr);
			dt->methods[labelName] = p.instructions.GetSize();
		}
	}

	Int callArgs = nodes.GetSize() - 1;

	bool isNativePropMethod = false;
	// for special native props
	bool wantTypeIndex = false;
	bool wantTypeSize = false;
	bool resElem = false;
	bool resSlice = false;
	bool firstArgElem = false;
	bool secondArgElem = false;

	if (fdef && fdef->type == AST_NPROP_METHOD)
	{
		wantTypeIndex = (fdef->flags & AST_F_PUSH_TYPE) != 0;
		wantTypeSize = (fdef->flags & AST_F_PUSH_TYPE_SIZE) != 0;
		firstArgElem = (fdef->flags & AST_F_ARG1_ELEM) != 0;
		secondArgElem = (fdef->flags & AST_F_ARG2_ELEM) != 0;
		resElem = (fdef->flags & AST_F_RES_ELEM) != 0;
		resSlice = (fdef->flags & AST_F_RES_SLICE) != 0;

		Int constFlags = fdef->qualifiers & AST_Q_CONST;
		// got native property...
		auto fnameNode = AstStaticCast<AstText *>(fdef);
		const auto &fnameText = fnameNode->text;
		const auto fci = p.nativeMap.Find(fnameText);

		if (fci == p.nativeMap.End())
			return p.Error(this, String::Printf("function not found:%s", fnameNode->text.Ansi()));

		fdef = fci->value;
		fname = fnameText;
		isMethod = true;
		fdef->qualifiers |= constFlags;

		isNativePropMethod = true;
	}

	if (!fdef)
		return p.Error(this, String::Printf("function not found:%s", nodes[0]->GetTextRepresentation().Ansi()));

	// don't generate assert call in release mode
	if ((fdef->qualifiers & AST_Q_ASSERT) && p.GetUnsafe())
		return true;

	if (fdef->qualifiers & AST_Q_INTRINSIC)
		return CodeGenIntrinsic(p, fdef);

	if ((fdef->qualifiers & (AST_Q_VIRTUAL | AST_Q_INLINE)) == (AST_Q_VIRTUAL | AST_Q_INLINE))
	{
		fdef->qualifiers ^= AST_Q_INLINE;
		p.Warning(fdef, "inline ignored for virtual methods");
	}

	if (fdef->type != AST_TYPE_FUNC_PTR)
	{
		QDataType qdt = fdef->GetTypeDesc(p);
		const auto dte = qdt.GetTypeEnum();

		if (dte != DT_FUNC_PTR && dte != DT_DELEGATE)
			return p.Error(this, "can only call a function");

		if (fdef->type != AST_FUNC && fdef->type != AST_VAR_DECL)
		{
			fdef = qdt.GetType().funcRef;
			if (!fdef)
				return p.Error(this, "invalid function type");
		}

		if (fdef->type == AST_VAR_DECL)
		{
			auto *tnode = fdef->parent->nodes[0];
			fdef = nullptr;

			if (tnode->type == AST_TYPE_FUNC_PTR || tnode->type == AST_TYPE_DELEGATE)
				fdef = tnode;
			else if (tnode->target)
			{
				// FIXME: horrible, needs refactoring!
				auto tdef = tnode->target;

				while (tdef && tdef->type == AST_TYPEDEF)
				{
					tdef = tdef->nodes[0];
				}

				if (tdef && (tdef->type == AST_TYPE_FUNC_PTR || tdef->type == AST_TYPE_DELEGATE))
					fdef = const_cast<AstNode *>(tdef);
			}

			if (!fdef)
				return p.Error(this, "function type not found");
		}
	}

	if (fdef->qualifiers & AST_Q_THREAD_UNSAFE)
	{
		// validate unsafe call
		if (auto *fnode = FindEnclosingFunction())
			if (fnode && (fnode->qualifiers & AST_Q_THREAD_CALL))
				return p.Error(this, String::Printf("attempting to call thread-unsafe function `%s'", fname.Ansi()));
	}

	AstFuncBase *fn = AstStaticCast<AstFuncBase *>(fdef);

	AstNode *args = fn->GetArgs();

	const AstNode *res = fn->GetResult();
	Int resultWords = 0;
	bool resultZeroed = false;

	QDataType resType;

	if (res->type != AST_TYPE_VOID)
	{
		if (fn->HasEllipsis() && !(fn->qualifiers & AST_Q_NATIVE))
			return p.Error(fn, "non-native functions with ellipsis must not return values (internal limitation)");

		// reserve result
		QDataType tdesc = res->GetTypeDesc(p);

		if ((tdesc.qualifiers & AST_Q_NODISCARD) && parent && parent->type == AST_EXPR)
			p.Warning(this, "discarding result of a function call", WARN_DISCARD);

		if (resElem | resSlice)
		{
			auto etdesc = nodes[0]->nodes[0]->GetTypeDesc(p);
			LETHE_ASSERT(etdesc.IsArray());

			if (resSlice)
			{
				tdesc = etdesc;

				if (tdesc.GetTypeEnum() == DT_DYNAMIC_ARRAY)
					tdesc.ref = tdesc.ref->complementaryType;

				tdesc.RemoveReference();
			}
			else
				tdesc = etdesc.GetType().elemType;
		}

		resType = tdesc;

		if (tdesc.IsReference())
		{
			resultWords = 1;
			// we always zero references here in debug mode
			resultZeroed = !p.GetUnsafe();
			p.EmitI24(p.GetUnsafe() ? OPC_PUSH_NOZERO : OPC_PUSHZ_RAW, 1);
		}
		else
		{
			resultWords = (tdesc.GetSize() + Stack::WORD_SIZE-1) / Stack::WORD_SIZE;

			// we have a new problem here: if func does nrvo and we don't zero-init here
			// and it does return without init, we get garbage! => force zero-init for nrvo without noinit

			bool forceZeroInit = (fn->flags & AST_F_NRVO) != 0 && !(tdesc.qualifiers & AST_Q_NOINIT);

			// force zero init on small numbers
			forceZeroInit = forceZeroInit || tdesc.GetTypeEnum() < DT_INT;

			resultZeroed = (forceZeroInit || tdesc.ZeroInit());

			p.EmitU24Zero(resultZeroed ? OPC_PUSHZ_RAW : OPC_PUSH_NOZERO, resultWords);

			p.EmitCtor(tdesc);
			resType.qualifiers &= ~AST_Q_SKIP_DTOR;
		}
	}

	if (p.curScope)
	{
		p.curScope->varOfs += resultWords * Stack::WORD_SIZE;
		p.curScope->varSize += resultWords * Stack::WORD_SIZE;
	}

	NamedScope cscope(NSCOPE_LOCAL);
	cscope.parent = p.curScope;
	p.EnterScope(&cscope);

	auto resultBaseOffset = p.curScope->varOfs - resultWords * Stack::WORD_SIZE;

	Int minArgs = fn->GetMinArgs();
	Int minValidArgs = fn->GetMinArgs(false);

	if (!fn->HasEllipsis() && callArgs > minArgs)
		return p.Error(this, String::Printf("too many arguments to call `%s'", fname.Ansi()));

	if (callArgs < minValidArgs)
		return p.Error(this, String::Printf("not enough arguments to call `%s'; got %d expected %d", fname.Ansi(), callArgs, minValidArgs));

	Array<DataTypeEnum> formatTypes;
	bool checkFormat = false;

	// format string arg index
	Int formatString = -1;
	auto *fnargs = fn->GetArgs();

	if (fn->qualifiers & AST_Q_FORMAT)
	{
		auto nargs = fnargs ? fnargs->nodes.GetSize() : 0;
		formatString = nargs-2;

		if (fn->HasEllipsis() && formatString >= 0 && args->nodes.GetSize() > formatString && args->nodes.GetSize() >= 2 && args->nodes[formatString]->type == AST_ARG)
		{
			if (!args->nodes[formatString]->nodes.IsEmpty())
			{
				auto ntype = args->nodes[formatString]->nodes[0]->type;
				checkFormat = (ntype == AST_TYPE_STRING || ntype == AST_TYPE_NAME);
			}
		}

		if (!checkFormat)
			return p.Error(this, "couldn't find format string");
	}

	if (checkFormat)
	{
		checkFormat = false;

		if (nodes.GetSize() > formatString+1)
		{
			auto ntype = nodes[formatString+1]->type;

			if (ntype == AST_CONST_STRING || ntype == AST_CONST_NAME)
			{
				AnalyzeFormatStr(AstStaticCast<AstText *>(nodes[formatString+1])->text, formatTypes);

				if (nodes.GetSize()-2-formatString != formatTypes.GetSize())
					return p.Error(this, String::Printf("format error: argument count mismatch (got %d expected %d)", nodes.GetSize()-2-formatString, formatTypes.GetSize()));

				checkFormat = true;
			}
		}
	}

	//int fullArgs = fn->HasEllipsis();
	Int needArgs = Max<Int>(callArgs, minArgs);

	// exprStackOfs for temp args
	StackArray<TempArg, 16> tempArgs;

	bool lastEllipsis = fn->HasEllipsis();

	// FIXME: refactor!!!
	// push args back to front
	for (Int i = needArgs; i > 0; i--)
	{
		// problem with references: must verify that types match!
		// ideally, I should create a temporary for expression, but I won't allow that (I should for const refs!)

		Int fidx = i-1;
		fidx = Min(fidx, args->nodes.GetSize()-1);

		const bool isEllipsis = args->nodes[fidx]->type == AST_ARG_ELLIPSIS;

		if (lastEllipsis && !isEllipsis)
		{
			Int ellipsisArgCount = needArgs-i;
			// push arg count
			p.EmitIntConst(ellipsisArgCount);
			cscope.AllocVar(QDataType::MakeConstType(p.elemTypes[DT_INT]));
		}

		lastEllipsis = isEllipsis;

		AstNode *argValue = i < nodes.GetSize() ? nodes[i] : args->nodes[fidx]->nodes[2];

		// check named args first
		if (fidx < namedArgs.GetSize())
		{
			const auto &narg = namedArgs[fidx];

			auto *argname = !isEllipsis ? args->nodes[fidx]->nodes[1] : nullptr;

			if (isEllipsis || (!narg.IsEmpty() && argname->type == AST_IDENT && narg != AstStaticCast<AstText *>(argname)->text))
				return p.Error(nodes[i], "named argument mismatch");
		}

		QDataType tdesc = args->nodes[fidx]->GetTypeDesc(p);

		if (tdesc.GetTypeEnum() == DT_DELEGATE && i >= nodes.GetSize())
		{
			// we can only allow arg-initialization of delegates to null (otherwise we compile with current this pointer which would be wrong)
			if (argValue->GetTypeDesc(p).GetTypeEnum() != DT_NULL)
				return p.Error(argValue, "delegates can be default-initialized with null only (script limitation)");
		}

		// handle special methods for dynamic arrays
		if ((firstArgElem && fidx == 0) || (secondArgElem && fidx == 1))
		{
			if (nodes[0]->type != AST_OP_DOT)
				return p.Error(nodes[0], "expected dot operator");

			auto etdesc = nodes[0]->nodes[0]->GetTypeDesc(p);
			LETHE_ASSERT(etdesc.IsArray());
			tdesc = etdesc.GetType().elemType;

			// this is necessary because we pass smart ptrs without refcounting!
			if (tdesc.IsPointer() && !tdesc.IsReference() && argValue->type != AST_CALL && argValue->type != AST_NEW)
				tdesc.qualifiers |= AST_Q_SKIP_DTOR;
		}

		// FIXME: hack? but necessary to avoid leaks
		FixPointerQualifiers(p, tdesc, argValue);

		bool isRef = tdesc.IsReference();

		// note: assume never ref for ellipsis
		if (isRef && !tdesc.NonRefTypesMatch(argValue->GetTypeDesc(p)))
			return p.Error(this, "reference types don't match");

		// perform special conversions of constants now
		if (!isEllipsis && !isRef)
		{
			auto tdst = tdesc.GetTypeEnum();
			auto tsrc = argValue->GetTypeDesc(p).GetTypeEnum();

			if (tdst != tsrc)
			{
				argValue = argValue->ConvertConstTo(tdst, p);
				LETHE_RET_FALSE(argValue);
			}
		}

		bool tempArg = false;

		if (isRef && !argValue->CanPassByReference(p))
		{
			if (!tdesc.IsConst())
				return p.Error(argValue, "cannot pass by non-const reference here");

			if (tdesc.qualifiers & AST_Q_NOTEMP)
				return p.Error(argValue, "cannot pass a temporary here");

			// creating temp (inline => must simulate inline call later)
			tempArg = true;
			tempArgs.Add({cscope.varOfs, 0});
			isRef = false;
			tdesc.RemoveReference();
		}

		LETHE_RET_FALSE(isRef ? argValue->CodeGenRef(p, tdesc.IsConst()) : argValue->CodeGen(p));

		if (p.exprStack.IsEmpty())
			return p.Error(argValue, "argument expression must return a value");

		auto top = p.exprStack.Back();

		if (!top.IsReference() && (top.qualifiers & AST_Q_NOCOPY) && !top.IsPointer())
			return p.Error(argValue, "cannot pass nocopy variable by value");

		if (isRef && !tdesc.IsConst())
		{
			auto adt = argValue->GetTypeDesc(p);

			if (adt.IsPointer() && (adt.qualifiers & AST_Q_NOCOPY))
				return p.Error(argValue, "cannot pass pointer arg as lvalue reference");
		}

		if (top.IsMethodPtr() && tdesc.GetTypeEnum() == DT_FUNC_PTR)
			return p.Error(argValue, "cannot pass method");

		if (checkFormat && i > formatString+1)
		{
			auto ftype = formatTypes[i-formatString-2];
			auto atype = top.GetTypeEnumUnderlying();

			if (!FormatTypeOk(atype, ftype))
				return p.Error(argValue, "format type mismatch");
		}

		// perform conversion if necessary
		// FIXME: this condition is super-ugly, must refactor!
		if (!isEllipsis && !isRef && (tdesc.IsPointer() ? tdesc.GetTypeEnum() != top.GetTypeEnum() || !tdesc.CanAssign(top) :
			(!tdesc.CanAlias(top) || tdesc.GetType() != top.GetType() || tdesc.GetTypeEnum() != top.GetTypeEnum())
		))
		{
			LETHE_RET_FALSE(p.EmitConv(argValue, top, tdesc));

			// FIXME: ugh, it's getting uglier and uglier
			// conversion can force skip dtor
			if (tdesc.IsPointer())
			{
				tdesc.qualifiers &= ~AST_Q_SKIP_DTOR;
				tdesc.qualifiers |= p.exprStack.Back().qualifiers & AST_Q_SKIP_DTOR;
			}
		}

		// tdesc is arg target in function
		auto argtype = isEllipsis ? argValue->GetTypeDesc(p) : tdesc;

		if (isEllipsis && argtype.IsArray() && argtype.GetTypeEnum() == DT_DYNAMIC_ARRAY)
		{
			argtype.ref = argtype.GetType().complementaryType;

			if (argValue->type == AST_CALL && !argtype.IsReference())
				return p.Error(argValue, "cannot convert dynamic array to array ref via a call");
		}

		if (isEllipsis)
		{
			if (argtype.IsArray() && argtype.GetTypeEnum() == DT_STATIC_ARRAY)
			{
				// convert to array ref
				LETHE_RET_FALSE(p.EmitConv(argValue, top, QDataType::MakeQType(*argtype.GetType().complementaryType, argtype.qualifiers)));
				argtype = top = p.exprStack.Back();
			}

			argtype.RemoveReference();

			// FIX leaks in ellipsis
			FixPointerQualifiers(p, argtype, argValue);
		}

		if (Endian::IsBig() && !isEllipsis && !argtype.IsReference() && argtype.IsSmallNumber())
		{
			// big endian adjust
			p.EmitI24(OPC_ISHL_ICONST, (4-argtype.GetSize())*8);
		}

		p.PopStackType(1);

		argValue->offset = cscope.AllocVar(argtype);

		if (isEllipsis)
		{
			if constexpr (Stack::WORD_SIZE < 8)
				p.EmitUIntConst((UInt)(UIntPtr)&argtype.GetType());
			else
				p.EmitULongConst((ULong)(UIntPtr)&argtype.GetType());

			cscope.AllocVar(QDataType::MakeConstType(p.elemTypes[Stack::WORD_SIZE < 8 ? DT_UINT : DT_ULONG]));
		}

		if (tempArg)
			tempArgs.Back().size = cscope.varOfs - tempArgs.Back().offset;
	}

	if (lastEllipsis)
	{
		Int ellipsisArgCount = needArgs;
		// push arg count
		p.EmitIntConst(ellipsisArgCount);
		cscope.AllocVar(QDataType::MakeConstType(p.elemTypes[DT_INT]));
	}

	// everything pushed...

	Int tempWordsPushed = 0;
	auto tempEnd = cscope.varOfs;

	if (!tempArgs.IsEmpty())
	{
		// create virtual copy (result, args...)
		tempWordsPushed = GenTempCopy(p, resultBaseOffset, resultWords, resultZeroed, tempEnd, tempArgs);
		// make sure chkstk has valid max
		cscope.maxVarSize += (tempWordsPushed+2)*Stack::WORD_SIZE;

		// temporarily adjust stack
		cscope.varOfs  += tempWordsPushed * Stack::WORD_SIZE;
		cscope.varSize += tempWordsPushed * Stack::WORD_SIZE;
	}

	bool noPopThis = false;

	QDataType structType;

	if (isMethod)
	{
		// first arg is this (reference); actually it's already pushed on stack as ref even before result!
		// this sucks a bit; meaning that we can't have methods with format-string
		// SO we have to do better
		// will use this as internal register... this means two things:
		// #1 need to inject this ptr as stack-relative register
		// #2 need pushthis/popthis opcodes OR even better a special method call opcode (impossible...)
		// #3 this lthis opcode to load ptr from this on stack
		// hmm... we don't need this register at all, actually... => will think about it... but a reg might help JIT

		// handle const-ness properly

		LETHE_ASSERT(fn->type != AST_TYPE_FUNC_PTR);

		if (nodes[0]->type == AST_IDENT)
		{
			// we need to handle a case here where we call another method from within a method

			// find parent func scope
			auto *pfn = FindEnclosingFunction();

			if (!pfn)
				return p.Error(this, "enclosing function not found");

			if ((pfn->qualifiers & AST_Q_CONST) && !(fn->qualifiers & AST_Q_CONST))
				return p.Error(this, "cannot call non-const method");

			if (!(pfn->qualifiers & AST_Q_METHOD))
				return p.Error(this, "illegal call of non-static method");

			const NamedScope *nscope = scopeRef->FindThis();
			const NamedScope *nscope2 = pfn->scopeRef->FindThis();

			// nscope2 must be a base of nscope
			if (!nscope2 || !nscope2->IsBaseOf(nscope))
				return p.Error(this, "illegal call of non-base method");

			LETHE_RET_FALSE(ValidateMethod(p, scopeRef->FindThis(), fn));

			// this simple optimization helps JIT a little
			if (nscope == nscope2 && p.GetJitFriendly())
			{
				noPopThis = true;
				p.EmitI24(OPC_PUSH_RAW, 1);
			}
			else
				p.Emit(OPC_PUSHTHIS);
		}
		else
		{
			LETHE_RET_FALSE(nodes[0]->CodeGenRef(p, (fn->qualifiers & AST_Q_CONST) != 0, 1));
			// now we have this on stack, let's load it and save previous one here
			structType = p.exprStack.Back();

			if (isNativePropMethod && !structType.AllowNativeProps())
				return p.Error(this, "invalid native prop call for type");

			p.Emit(OPC_LOADTHIS);
			p.PopStackType(1);
		}
	}

	// emit call
	if (fn->type == AST_TYPE_DELEGATE)
	{
		p.Emit(OPC_PUSHTHIS);
		p.PushStackType(QDataType::MakeConstType(p.elemTypes[DT_FUNC_PTR]));
		// call delegate
		LETHE_RET_FALSE(nodes[0]->CodeGen(p));
		p.Emit(OPC_LOADTHIS_IMM);
		p.EmitI24(OPC_POP, 1);
		p.Emit(OPC_FCALL_DG);
		p.PopStackType(1);
		p.PopStackType(1);
		p.Emit(OPC_POPTHIS);
	}
	else if (fn->type == AST_TYPE_FUNC_PTR)
	{
		// call function ptr
		LETHE_RET_FALSE(nodes[0]->CodeGen(p));
		p.Emit(OPC_FCALL);
		p.PopStackType(1);
	}
	else
	{
		// standard function

		bool forceVirtual = (fn->qualifiers & AST_Q_VIRTUAL) && !(nodes[0]->GetTypeDesc(p).qualifiers & AST_Q_NON_VIRT);

		if (!forceVirtual && (fn->qualifiers & AST_Q_NATIVE))
		{
			// native call => collect prefixed names (namespaces)
			const NamedScope *nscope = fdef->scopeRef;

			StringBuilder sb;
			sb += fname;

			while (nscope)
			{
				if (!nscope->name.IsEmpty())
				{
					sb.Prepend("::");
					sb.Prepend(nscope->name);

					// note: this is necessary for instantiated templates because they are fully resolved
					if (nscope->name.Find("::") >= 0)
						break;
				}

				nscope = nscope->parent;
			}

			fname = sb.Get();

			Int fidx = p.cpool.FindNativeFunc(fname);

			if (fidx < 0)
				return p.Error(this, String::Printf("native function not found: `%s'", fname.Ansi()));

			if (wantTypeIndex | wantTypeSize)
			{
				LETHE_ASSERT(structType.IsArray());
				const auto &etype = structType.GetType().elemType.GetType();
				p.EmitIntConst(wantTypeIndex ? etype.typeIndex : etype.size);
			}

			p.ProfEnter(fname);
			p.EmitI24((isMethod ? OPC_NMCALL : OPC_NCALL), fidx);
			p.ProfExit();

			if (wantTypeIndex | wantTypeSize)
			{
				p.EmitI24(OPC_POP, 1);
			}
		}
		else
		{
			// script call
			AstFunc *fun = AstStaticCast<AstFunc *>(fn);

			if ((fun->qualifiers & AST_Q_INLINE) && p.InlineExpansionAllowed())
			{
				if (p.GetInline() > 10)
					return p.Error(this, "inline nesting too deep");

				Int oldCodeSize = p.instructions.GetSize();
				p.SetInline(+1);

				// must save exprStackOfs here
				auto oldOfs = p.exprStackOfs;

				// also return handles
				Array<Int> oretHandles;
				oretHandles.Clear();
				Swap(oretHandles, p.GetReturnHandles());

				if (!p.IsFastCall() && !p.GetInline())
					p.EmitI24Zero(OPC_PUSH_RAW, 1);

				// FIXME: this causes lmove opt to break, must investigate later => zeroing for now
				p.exprStackOfs = 0;// - !p.IsFastCall() * Stack::WORD_SIZE;

				auto odelta = p.initializerDelta;
				p.initializerDelta = 0;

				bool igen = fun->CodeGen(p);

				p.initializerDelta = odelta;

				if (!p.IsFastCall() && !p.GetInline())
					p.EmitI24(OPC_POP, 1);

				// restore exprStackOfs
				p.exprStackOfs = oldOfs;

				// restore return handles
				Swap(oretHandles, p.GetReturnHandles());

				p.SetInline(-1);

				if (!p.GetInline() && p.instructions.GetSize() - oldCodeSize > 256)
					return p.Error(this, "inlined code too big (>256 opcodes)");

				LETHE_RET_FALSE(igen);
			}
			else if (forceVirtual)
				p.EmitI24(OPC_VCALL, fun->vtblIndex);
			else
			{
				if (fun->offset >= 0)
					p.EmitBackwardJump(OPC_CALL, fun->offset);
				else
					fun->AddForwardRef(p.EmitForwardJump(OPC_CALL));
			}
		}

	}

	// post-call

	if (isMethod)
	{
		if (noPopThis)
			p.EmitI24(OPC_POP, 1);
		else
			p.Emit(OPC_POPTHIS);
	}

	if (tempWordsPushed)
	{
		if (resultWords > 0)
		{
			// copy result
			auto delta = (tempEnd - resultBaseOffset)/Stack::WORD_SIZE;

			// src adr
			auto srcAdr = tempWordsPushed - resultWords;
			// dst adr
			auto dstAdr = 1+delta+tempWordsPushed-resultWords;

			if (resultWords == 1)
			{
				p.EmitI24(OPC_LPUSHPTR, srcAdr);
				p.EmitI24(OPC_LSTOREPTR, dstAdr);
			}
			else
			{
				p.EmitI24(OPC_LPUSHADR, srcAdr);
				p.EmitI24(OPC_LPUSHADR, dstAdr);
				p.EmitI24(OPC_PCOPY, resultWords * Stack::WORD_SIZE);
			}
		}

		// and then simply clean up
		p.EmitI24(OPC_POP, tempWordsPushed);

		if (p.curScope)
		{
			p.curScope->varOfs  -= tempWordsPushed * Stack::WORD_SIZE;
			p.curScope->varSize -= tempWordsPushed * Stack::WORD_SIZE;
		}
	}

	LETHE_RET_FALSE(p.LeaveScope());

	// now I should PushStackType (result)
	if (res->type != AST_TYPE_VOID)
	{
		// reserve result
		if (!resType.IsReference() || keepRef)
		{
			p.PushStackType(resType);

			if (derefPtr && resType.IsReference() && resType.IsPointer())
				p.Emit(OPC_PLOADPTR_IMM);
		}
		else
			LETHE_RET_FALSE(EmitPtrLoad(resType, p));

		if (!resType.IsReference() && parent && parent->type == AST_EXPR)
		{
			// technically we should just delete the array, but it'd be complicated and dumb so we rather abort
			// with an error
			if (resType.GetTypeEnum() == DT_DYNAMIC_ARRAY)
				return p.Error(this, "illegal expression construct");
		}
	}

	if (Endian::IsBig() && !resType.IsReference() && resType.IsSmallNumber())
	{
		// endian-adjust result after call
		p.EmitI24(resType.GetTypeEnum() == DT_SHORT || resType.GetTypeEnum() == DT_SBYTE ?
			OPC_ISAR_ICONST : OPC_ISHR_ICONST, (4-resType.GetSize())*8);
	}

	if (p.curScope)
	{
		p.curScope->varOfs -= resultWords * Stack::WORD_SIZE;
		p.curScope->varSize -= resultWords * Stack::WORD_SIZE;
	}


	if (isLatent)
	{
		// if result is false, simply return
		p.EmitI24(OPC_IBNZ, 1);
		p.EmitI24(OPC_RET, 0);
		p.FlushOpt();
	}
	else if (isStateBreak && !p.StateBreakScope())
		return p.Error(this, "state break call failed (recursion via defer?)");

	if (isStateFuncCall)
	{
		p.ReturnScope(false);
		p.Emit(OPC_RET);
	}

	return true;
}

bool AstCall::CallOperator(Int arity, CompiledProgram &p, AstNode *n, AstNode *op, bool isRef)
{
	AstCall tmp(n->location);
	tmp.parent = n->parent;

	tmp.scopeRef = n->scopeRef;
	tmp.nodes.Resize(1+arity);
	tmp.nodes[0] = nullptr;

	for (Int i=0; i<arity; i++)
		tmp.nodes[1+i] = n->nodes[i];

	tmp.forceFunc = op;
	bool res = isRef ? tmp.CodeGenRef(p) : tmp.CodeGen(p);
	tmp.nodes.Clear();
	tmp.parent = nullptr;
	return res;
}

void AstCall::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstCall *>(n);
	tmp->forceFunc = forceFunc;
	tmp->namedArgs = namedArgs;
}


}
