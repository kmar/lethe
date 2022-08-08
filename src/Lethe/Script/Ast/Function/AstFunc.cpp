#include "AstFunc.h"
#include "AstCall.h"
#include "../NamedScope.h"
#include "../AstExpr.h"
#include <Lethe/Script/Ast/ControlFlow/AstReturn.h>
#include <Lethe/Script/Ast/AstSymbol.h>
#include <Lethe/Script/Ast/BinaryOp/AstBinaryAssignAllowConst.h>
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Stack.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/Compiler/Warnings.h>

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(AstFunc)

// AstFunc

QDataType AstFunc::GetTypeDesc(const CompiledProgram &) const
{
	return typeRef;
}

const AstNode *AstFunc::GetTypeNode() const
{
	return nodes[0]->target;
}

void AstFunc::AddForwardRef(Int handle)
{
	forwardRefs.Add(handle);
}

AstNode::ResolveResult AstFunc::Resolve(const ErrorHandler &e)
{
	if (!ResolveNode(e))
	{
		e.Error(this, "cannot resolve function node");
		return RES_ERROR;
	}

	AstNode *targ = nodes[IDX_NAME]->target;

	if (nodes[IDX_NAME]->type == AST_OP_SCOPE_RES)
	{
		if (!targ)
		{
			for (Int i=0; i<IDX_BODY; i++)
				if (nodes[i]->Resolve(e) == RES_ERROR)
					return RES_ERROR;

			return RES_OK;
		}
	}

	if (qualifiers & AST_Q_CTOR)
	{
		targ = this;

		// need noderef for ctor because of default init statements
		if (IsValidArrayIndex(IDX_BODY, nodes.GetSize()))
		{
			if (auto *sref = nodes[IDX_BODY]->scopeRef)
				sref->node = this;
		}
	}

	if (targ && targ != this)
	{
		// we must have a body
		if (IDX_BODY >= nodes.GetSize())
		{
			e.Error(nodes[IDX_NAME], "function body missing");
			return RES_ERROR;
		}

		// const must match
		if ((targ->qualifiers ^ qualifiers) & AST_Q_CONST)
		{
			e.Error(nodes[IDX_NAME], "qualifiers don't match");
			return RES_ERROR;
		}

		if (targ->type != AST_FUNC)
		{
			e.Error(nodes[IDX_NAME], "invalid method name");
			return RES_ERROR;
		}

		if (IDX_BODY < targ->nodes.GetSize())
		{
			e.Error(nodes[IDX_NAME], "method already has a body");
			return RES_ERROR;
		}

		// TODO: now try to match function signature: all nodes MUST match (sigh)

		targ->nodes[IDX_NAME]->location = nodes[IDX_NAME]->location;
		targ->location = location;

		// still have to replace all return assignments!
		AstIterator ai(nodes[IDX_BODY]);

		LETHE_ASSERT(scopeRef && targ->scopeRef);

		while (auto n = ai.Next())
		{
			if (n->target == scopeRef->resultPtr)
			{
				// remap return
				n->target = targ->scopeRef->resultPtr;
			}
		}


		nodes[IDX_BODY]->parent = targ;
		nodes[IDX_BODY]->scopeRef->parent = targ->nodes[IDX_ARGS]->scopeRef;
		nodes[IDX_BODY]->scopeRef->node = targ;

		// recheck variable shadowing
		const auto *sref = nodes[IDX_BODY]->scopeRef;

		StackArray<const NamedScope *, 256> scopeStack;

		scopeStack.Add(sref);

		while (!scopeStack.IsEmpty())
		{
			sref = scopeStack.Back();
			scopeStack.Pop();

			for (auto &&it : sref->scopes)
				scopeStack.Add(it);

			for (auto &&it : sref->members)
				if (it.value->type == AST_VAR_DECL)
					ErrorHandler::CheckShadowing(sref, it.key, it.value, e.onWarning);
		}

		targ->nodes.Add(nodes[IDX_BODY]);
		targ->flags &= ~AST_F_RESOLVED;
		nodes.EraseIndex(IDX_BODY);
		flags |= AST_F_SKIP_CGEN | AST_F_RESOLVED;

		AstNode *dummy = new AstNode(AST_NONE, location);
		dummy->flags |= AST_F_RESOLVED;
		parent->ReplaceChild(this, dummy);

		auto res = RES_MORE;

		if (targ->Resolve(e) == RES_ERROR)
			res = RES_ERROR;

		delete this;
		return res;
	}

	return Super::Resolve(e);
}

bool AstFunc::ValidateSignature(const AstFunc &o, const CompiledProgram &p) const
{
	if (nodes[IDX_ARGS]->nodes.GetSize() != o.nodes[IDX_ARGS]->nodes.GetSize())
		return p.Error(this, "argument count mismatch for virtual method");

	if (!nodes[IDX_RET]->GetTypeDesc(p).TypesMatch(o.nodes[IDX_RET]->GetTypeDesc(p)))
		return p.Error(nodes[IDX_RET], "result type mismatch for virtual method");

	const auto &nodes1 = nodes[IDX_ARGS]->nodes;
	const auto &nodes2 = o.nodes[IDX_ARGS]->nodes;

	for (Int i=0; i<nodes1.GetSize(); i++)
		if (!nodes1[i]->GetTypeDesc(p).TypesMatch(nodes2[i]->GetTypeDesc(p)))
			return p.Error(nodes1[i], "argument type mismatch for virtual method");

	return true;
}

bool AstFunc::ValidateADLCall(const AstCall &o) const
{
	Int callArgCount = o.nodes.GetSize()-1;

	Int argCount = nodes[IDX_ARGS]->nodes.GetSize();

	if (argCount != callArgCount)
	{
		// we can still match if it matches non-default argument count
		Int nonDefault = 0;

		for (auto *it : nodes[IDX_ARGS]->nodes)
		{
			if (it->nodes.GetSize() > 2)
				break;

			++nonDefault;
		}

		if (callArgCount < nonDefault || callArgCount > argCount)
			return false;
	}

	const auto &nodes1 = nodes[IDX_ARGS]->nodes;

	for (Int i=0; i<callArgCount; i++)
	{
		auto *tn0 = nodes1[i]->GetTypeNode();
		LETHE_RET_FALSE(tn0);

		auto *tn1 = o.nodes[i+1]->GetTypeNode();
		LETHE_RET_FALSE(tn1);

		auto *n0 = nodes1[i];
		LETHE_ASSERT(n0->type == AST_ARG);
		const bool isref = (n0->nodes[0]->qualifiers & AST_Q_REFERENCE) != 0;

		if (isref && tn0->type != tn1->type)
			return false;

		if (tn0->IsElemType() && tn1->IsElemType())
		{
			if (PromoteSmallType(tn0->type) != PromoteSmallType(tn1->type))
				return false;

			continue;
		}

		// validate array ref types
		auto arr0 = tn0->type == AST_TYPE_ARRAY_REF || tn0->type == AST_TYPE_DYNAMIC_ARRAY;
		auto arr1 = tn1->type == AST_TYPE_ARRAY_REF || tn1->type == AST_TYPE_DYNAMIC_ARRAY;

		if (arr0 == arr1 && arr1)
		{
			auto *el0 = tn0->nodes[0]->GetTypeNode();
			auto *el1 = tn1->nodes[0]->GetTypeNode();

			if (el0 && el1)
			{
				if (el0 == el1)
					continue;

				if (el0->IsElemType() && el1->IsElemType() && el0->type == el1->type)
					continue;
			}
		}

		if (tn0 != tn1)
			return false;
	}

	return true;
}

bool AstFunc::TypeGen(CompiledProgram &p)
{
	LETHE_RET_FALSE(Super::TypeGen(p));

	if (qualifiers & AST_Q_LATENT)
	{
		if ((qualifiers & (AST_Q_METHOD | AST_Q_STATIC)) != AST_Q_METHOD)
			return p.Error(this, "latent function must be non-static method");

		auto qdt = nodes[IDX_RET]->GetTypeDesc(p);

		if (qdt.GetTypeEnum() != DT_BOOL || qdt.IsReference())
			return p.Error(this, "latent functions must return bool");
	}

	if ((qualifiers & (AST_Q_CONST | AST_Q_STATIC)) == (AST_Q_CONST | AST_Q_STATIC))
		return p.Error(this, "static and const funcs are mutually exclusive");

	if ((qualifiers & (AST_Q_CONST | AST_Q_METHOD)) == AST_Q_CONST)
		return p.Error(this, "only methods can be marked as const");

	typeRef.qualifiers = qualifiers;
	typeRef.ref = p.AddType(GenFuncType(nullptr, p, nodes[0], nodes[2]->nodes));

	if ((qualifiers & (AST_Q_CTOR | AST_Q_NATIVE)) == AST_Q_CTOR)
	{
		LETHE_ASSERT(scopeRef && (qualifiers & AST_Q_METHOD));
		auto tscope = scopeRef->FindThis();
		LETHE_ASSERT(tscope && tscope->node);
		auto snode = tscope->node;

		// okay, now we should extract all var decls (struct auto-init-hack)
		AstNode *fbody = nodes[IDX_BODY];
		// virtual node to hold def inits so that we can place them before manual ctor body, also avoids inserting at index 0 (perf)
		AstNode *defInitRoot = nullptr;

		auto defInitCreateRoot = [&]()
		{
			if (!defInitRoot)
			{
				defInitRoot = new AstNode(AST_NONE, location);
				defInitRoot->flags |= AST_F_RESOLVED;
				fbody->nodes.InsertIndex(0, defInitRoot);
				defInitRoot->parent = fbody;
			}
		};

		auto defInit = [&](AstNode *n, Int ofs)
		{
			// okay, so nodes[ofs] = ident
			// nodes[ofs+1] = expr

			auto nloc = n->nodes[ofs]->location;

			if (n->nodes[ofs]->type != AST_IDENT)
				return;

			auto *expr = n->nodes[ofs+1];

			// init optimization
			if (n->type == AST_VAR_DECL && expr->IsZeroConstant(p))
				return;

			AstNode *asgn = new AstBinaryAssignAllowConst(nloc);
			AstSymbol *resIdent = new AstSymbol("", nloc);
			// must copy bitfield flag here
			resIdent->qualifiers |= n->nodes[ofs]->qualifiers & AST_Q_BITFIELD;
			resIdent->text = AstStaticCast<AstText *>(n->nodes[ofs])->text;
			// FIXME: for multi-default initializers, symbol target is null except for the first one
			// this is odd any might hide a bug
			resIdent->target = n->type == AST_VAR_DECL ? n : n->nodes[ofs]->target;
			// must be resolved!
			LETHE_ASSERT(resIdent->target);
			resIdent->flags |= AST_F_RESOLVED;
			resIdent->scopeRef = n->nodes[ofs]->scopeRef;
			resIdent->symScopeRef = n->nodes[ofs]->symScopeRef;

			asgn->Add(resIdent);
			expr->parent = nullptr;
			asgn->Add(expr);
			n->nodes.Resize(ofs+1);

			auto tmp = new AstExpr(nloc);
			tmp->Add(asgn);

			defInitCreateRoot();
			defInitRoot->Add(tmp);
		};

		for (Int i=2; i<snode->nodes.GetSize(); i++)
		{
			AstNode *tn = snode->nodes[i];

			if (tn->type == AST_DEFAULT_INIT)
			{
				for (auto *it : tn->nodes)
				{
					// fixup scope chain
					if (it->scopeRef == snode->scopeRef)
						it->scopeRef = fbody->scopeRef;
					else if (it->scopeRef && it->scopeRef->parent == snode->scopeRef)
						it->scopeRef->parent = fbody->scopeRef;

					defInitCreateRoot();
					it->parent = nullptr;
					defInitRoot->Add(it);
				}

				tn->nodes.Clear();
				continue;
			}

			if (tn->type != AST_VAR_DECL_LIST)
				continue;

			const auto *nt = tn->nodes[0];

			for (Int j=1; j<tn->nodes.GetSize(); j++)
			{
				AstNode *n = tn->nodes[j];

				if (n->nodes.GetSize() < 2)
					continue;

				// this is because of const name = expr;
				if ((nt->qualifiers & AST_Q_CONST) && n->nodes[1]->IsConstant())
					continue;

				if (!(nt->qualifiers & AST_Q_STATIC))
					defInit(n, 0);
			}
		}

		// invalidate cache indices for consistency; if needed
		if (defInitRoot)
			for (auto *it : fbody->nodes)
				it->cachedIndex = -1;
	}

	if (qualifiers & AST_Q_NATIVE)
	{
		// register native func node
		auto fname = AstStaticCast<AstText *>(nodes[IDX_NAME])->GetQText(p);
		p.nativeMap[fname] = this;
	}

	auto tdesc = nodes[IDX_RET]->GetTypeDesc(p);

	if ((tdesc.qualifiers & AST_Q_NOINIT) && tdesc.GetTypeEnum() < DT_INT && tdesc.GetTypeEnum() > DT_NONE)
		p.Warning(this, "noinit ignored for elementary types smaller than int", WARN_NOINIT_SMALL);

	return true;
}

bool AstFunc::AnalyzeFlow(CompiledProgram &p, Int startPC) const
{
	p.MarkReturnValue();

	p.InitValidReturnPath(startPC, p.instructions.GetSize());

	const bool mustReturn = nodes[IDX_RET]->GetTypeDesc(p).GetTypeEnum() != DT_NONE;

	// analyzing jump targets...
	auto ci = LowerBound(p.barriers.Begin(), p.barriers.End(), startPC);

	if (ci == p.barriers.End())
		return p.Error(this, "broken start PC");

	Int barrier = (Int)IntPtr(ci - p.barriers.Begin());
	Int nextBarrier = p.barriers[barrier++];

	Int returnIndex = 0;
	Int nextReturn = p.returnValues[returnIndex++];

	bool inPath = true;

	for (Int i=startPC; i<p.instructions.GetSize(); i++)
	{
		LETHE_ASSERT(nextBarrier < 0 || i <= nextBarrier);
		LETHE_ASSERT(nextReturn < 0 || i <= nextReturn);

		if (i == nextBarrier)
		{
			inPath = true;

			if (barrier < p.barriers.GetSize())
				nextBarrier = p.barriers[barrier];
			else
				nextBarrier = -1;

			++barrier;

			if (p.IsValidReturnPathTarget(i))
				inPath = false;
		}

		auto opc = p.instructions[i] & 255;

		if (i == nextReturn)
		{
			nextReturn = p.returnValues[returnIndex++];
			inPath = false;
		}

		if (opc == OPC_SWITCH)
		{
			auto range = p.instructions[i] >> 8;
			i += range + 1;
			continue;
		}

		if (opc == OPC_BR)
		{
			if (!inPath)
				p.MarkValidReturnPathTarget(i + 1 + (p.instructions[i]>>8));

			inPath = false;
		}
	}

	if (inPath && mustReturn)
		return p.Error(this, "not all paths return a value");

	return true;
}

bool AstFunc::CodeGen(CompiledProgram &p)
{
	if ((flags & AST_F_SKIP_CGEN) && !(qualifiers & (AST_Q_FUNC_REFERENCED | AST_Q_CTOR | AST_Q_DTOR)))
		return true;

	if (!(qualifiers & AST_Q_NATIVE) && IDX_BODY >= nodes.GetSize())
		return p.Error(nodes[IDX_NAME], "function body not defined");

	if (qualifiers & AST_Q_STATE)
	{
		if (!nodes[IDX_ARGS]->nodes.IsEmpty())
			return p.Error(nodes[IDX_ARGS], "state functions cannot have arguments passed to them");

		if (nodes[IDX_RET]->GetTypeDesc(p).GetTypeEnum() != DT_NONE)
			return p.Error(nodes[IDX_RET], "state functions cannot return values");
	}

	// [0] = ret type, [1] = name, [2] = arglist, [3] = (optional) body for non-native
	// emit function...
	auto *ftext = AstStaticCast<const AstText *>(nodes[IDX_NAME]);
	const String &fname = ftext->GetQText(p);

	Int startPC = -1;

	// TODO: we'll need to know more (later)
	if (!p.GetInline())
	{
		bool isOperator = (nodes[IDX_NAME]->qualifiers & AST_Q_OPERATOR) != 0;

		if (isOperator)
			p.EmitFunc(fname + " " + typeRef.GetName(), this, typeRef.GetName());
		else
		{
			if (qualifiers & AST_Q_CTOR)
				p.EmitFunc(fname + "$ctor", this, typeRef.GetName());
			else
				p.EmitFunc(fname, this, typeRef.GetName());
		}

		startPC = p.instructions.GetSize();

		auto thisScope = scopeRef->FindThis();

		if (thisScope && !(qualifiers & (AST_Q_CTOR | AST_Q_DTOR /*| AST_Q_NATIVE*/)))
		{
			auto qdt = thisScope->node->GetTypeDesc(p);
			auto dt = const_cast<DataType *>(qdt.ref);

			const auto fnamePlain = AstStaticCast<AstText *>(nodes[IDX_NAME])->text;
			Int midx = vtblIndex >= 0 ? -vtblIndex : p.instructions.GetSize();
			dt->methods[fnamePlain] = midx;
		}

		// try to handle special __cmp function
		if (ftext->text == "__cmp")
		{
			auto args = GetArgs();

			if (!thisScope || thisScope->type != NSCOPE_STRUCT)
				return p.Error(nodes[IDX_NAME], "__cmp can only be defined inside a struct");

			if ((qualifiers & (AST_Q_NATIVE | AST_Q_STATIC)) != AST_Q_STATIC)
				return p.Error(nodes[IDX_NAME], "invalid __cmp qualifiers (must be non-native static)");

			if (args->nodes.GetSize() != 2)
				return p.Error(nodes[IDX_NAME], "__cmp takes two arguments");

			auto rtype = nodes[IDX_RET]->GetTypeDesc(p);

			if (rtype.IsReference() || rtype.GetTypeEnum() != DT_INT)
				return p.Error(nodes[IDX_NAME], "__cmp must return an int");

			LETHE_ASSERT(thisScope->node);
			auto stype = thisScope->node->GetTypeDesc(p);

			// verify cmp takes const reference to struct type
			for (auto &&n : args->nodes)
			{
				auto ntype = n->GetTypeDesc(p);
				if ((ntype.qualifiers & (AST_Q_CONST | AST_Q_REFERENCE)) != (AST_Q_CONST | AST_Q_REFERENCE))
					return p.Error(n, "__cmp args must be const reference");

				if (!(ntype.GetType() == stype.GetType()))
					return p.Error(n, "invalid __cmp arg type");
			}

			stype.ref->funCmp = startPC;
		}

		// fixup forward refs
		LETHE_RET_FALSE(p.FixupHandles(forwardRefs));
	}

	// enter new scope (args)
	p.EnterScope(nodes[2]->scopeRef);

	// FIXME: better (native check)
	if (!p.GetInline() && !p.GetUnsafe() && nodes.GetSize() > 3)
	{
		p.curScope->chkStkIndex = p.instructions.GetSize();
		p.Emit(OPC_CHKSTK);
	}

	// check return types and flow
	// problem with flow analysis: infinite loops, ...

	AstNode *nrvo = nullptr;
	bool noNrvo = false;
	// crude check for return
	bool hasReturnValue = false;

	if (nodes.GetSize() > 3)
	{
		AstConstIterator ci(nodes[3]);
		const AstNode *vn;
		QDataType retType = nodes[0]->GetTypeDesc(p);

		while ((vn = ci.Next()) != nullptr)
		{
			if (vn->type != AST_RETURN && vn->type != AST_RETURN_VALUE)
				continue;

			if (vn->flags & AST_F_NRVO)
			{
				hasReturnValue = true;
				break;
			}

			const AstReturn *ret = AstStaticCast<const AstReturn *>(vn);
			// structure: return => <expr => assign>
			QDataType tmpType = ret->nodes.IsEmpty() ? ret->GetTypeDesc(p) : ret->nodes[0]->nodes[0]->GetTypeDesc(p);
			bool tvoid = tmpType.GetTypeEnum() == DT_NONE;
			bool rvoid = retType.GetTypeEnum() == DT_NONE;

			if (tvoid != rvoid)
				return p.Error(ret, "invalid return type");

			if (!tvoid)
				hasReturnValue = true;

			if (ret->nodes.IsEmpty())
			{
				noNrvo = true;
				break;
			}

			const auto &expr = ret->nodes[0];

			if (expr->type == AST_STRUCT_LITERAL)
			{
				noNrvo = true;
				break;
			}

			const auto &rhs = expr->nodes[0]->nodes[1];

			auto rhsType = rhs->GetTypeDesc(p);

			if (rhs->type != AST_IDENT || (rhsType.qualifiers & AST_Q_STATIC))
			{
				noNrvo = true;
				break;
			}

			auto targ = rhs->target;

			if (targ && (!targ->scopeRef->IsLocal() || (targ->qualifiers & AST_Q_STATIC)))
			{
				noNrvo = true;
				break;
			}

			if (!nrvo)
				nrvo = targ;
			else if (nrvo != targ)
			{
				noNrvo = true;
				break;
			}
		}
	}

	// first do a virtual push of result
	if (nodes[0]->type != AST_TYPE_VOID)
	{
		QDataType rdt = nodes[0]->GetTypeDesc(p);

		if (rdt.IsReference())
			noNrvo = true;

		nodes[0]->offset = p.curScope->AllocVar(rdt);

		if (!hasReturnValue && !(qualifiers & AST_Q_NATIVE))
			return p.Error(this, "function must return a value");
	}

	if (noNrvo)
		nrvo = nullptr;

	if (nrvo && nrvo->nodes.GetSize() > 1)
	{
		// if we have initializer, only accept if it's initializer list
		if (nrvo->nodes[1]->type != AST_INITIALIZER_LIST)
			nrvo = nullptr;
	}

	if (nrvo)
	{
		auto retType = nodes[0]->GetTypeDesc(p);
		auto nrvoType = nrvo->GetTypeDesc(p);

		AstIterator ci(nodes[3]);
		AstNode *vn;
		Array<AstNode *> rets;

		if (!retType.CanAssign(nrvoType) || (!retType.IsNumber() && retType.ref != nrvoType.ref))
		{
			p.Warning(nrvo, "nrvo prevented due to incompatible types", WARN_NRVO_PREVENTED);
			nrvo = nullptr;
			goto skipNrvo;
		}

		// mark func as NRVO
		flags |= AST_F_NRVO;
		// nrvo-optimize returns!!!
		nrvo->flags |= AST_F_NRVO;
		nrvo->offset = nodes[0]->offset;
		nrvo->target = nodes[0];

		while ((vn = ci.Next()) != 0)
		{
			if (vn->type != AST_RETURN && vn->type != AST_RETURN_VALUE)
				continue;

			// disable nrvo for derived structs returned by value
			if (vn->type == AST_RETURN && !retType.IsReference() && retType.IsStruct())
			{
				// expr/assign
				auto *asgn = vn->nodes[0]->nodes[0];

				if (asgn->nodes.GetSize() > 1 && !retType.CanAlias(asgn->nodes[1]->GetTypeDesc(p)))
				{
					if (retType.IsStruct())
						return p.Error(asgn, String::Printf("cannot return %s derived struct by value", retType.GetName().Ansi()));

					continue;
				}
			}

			rets.Add(vn);
		}

		for (int i=0; i<rets.GetSize(); i++)
		{
			rets[i]->ClearNodes();
			rets[i]->type = AST_RETURN_VALUE;
			rets[i]->flags |= AST_F_NRVO;
		}
skipNrvo:;
	}

	// virtual-push args
	AstNode *alist = nodes[2];

	for (Int i=alist->nodes.GetSize()-1; i >= 0; i--)
	{
		AstNode *arg = alist->nodes[i];

		if (arg->type == AST_ARG_ELLIPSIS || arg->type == AST_TYPE_VOID)
			continue;

		auto qdt = arg->GetTypeDesc(p);
		arg->offset = p.curScope->AllocVar(qdt);
		// [0] = type, [1] = name, [2] (optional) = init
	}

	QDataType virt;
	virt.qualifiers |= AST_Q_REFERENCE;

	if (qualifiers & AST_Q_METHOD)
	{
		// virtual-alloc this
		p.curScope->AllocVar(virt);
	}

	// virtual-alloc return addr
	if (!p.IsFastCall() && !p.GetInline() && !(qualifiers & AST_Q_NATIVE))
		p.curScope->AllocVar(virt);

	if (!p.GetInline())
	{
		p.StartStackFrame();

		if (!(qualifiers & AST_Q_NATIVE))
			p.ProfEnter(fname);
	}

	// clear latent counter here
	const bool isStateFunc = (qualifiers & AST_Q_STATE) != 0;

	if (isStateFunc && !p.GetInline())
		p.ClearLatentCounter();

	for (Int i = alist->nodes.GetSize() - 1; i >= 0; i--)
	{
		AstNode *arg = alist->nodes[i];

		if (arg->type == AST_ARG_ELLIPSIS || arg->type == AST_TYPE_VOID)
			continue;

		if (!(qualifiers & AST_Q_NATIVE))
		{
			auto qdt = arg->GetTypeDesc(p);
			p.StartLocalVarLifeTime(qdt, arg->offset, AstStaticCast<const AstText *>(arg->nodes[1])->text);
		}
	}

	if (nrvo)
		p.StartLocalVarLifeTime(nodes[0]->GetTypeDesc(p), nrvo->offset, AstStaticCast<AstText *>(nrvo->nodes[0])->text);

	if (nodes.GetSize() > IDX_BODY)
	{
		LETHE_RET_FALSE(nodes[IDX_BODY]->CodeGen(p));

		if (!p.GetInline())
			LETHE_RET_FALSE(AnalyzeFlow(p, startPC));
	}
	else
	{
		// virtual native wrapper

		auto fidx = p.cpool.FindNativeFunc(fname);

		if (fidx < 0)
		{
			// generate exception
			p.Emit(OPC_PUSH_ICONST);
			p.Emit(OPC_PUSH_ICONST);
			p.Emit(OPC_IDIV);
		}
		else
		{
			p.ProfEnter(fname);

			if (!p.IsFastCall())
				p.EmitI24(OPC_POP, 1);

			// forcing to use NMCALL because of JIT since it stores this from reg to stack struct
			p.EmitI24(OPC_NMCALL, fidx);

			if (!p.IsFastCall())
				p.EmitI24(OPC_PUSH_RAW, 1);

			p.ProfExit();
		}
		p.Emit(OPC_RET);
	}

	if (p.curScope->chkStkIndex >= 0)
	{
		// fixup
		Int stkLimit = p.curScope->maxVarSize;
		stkLimit += Stack::WORD_SIZE-1;
		stkLimit /= Stack::WORD_SIZE;
		// arbitrary extra reserve (don't forget this and retadr) and calling back to VM
		stkLimit += 5;

		// check for stkLimit overflow
		LETHE_RET_FALSE(stkLimit <= (1 << 24)-1);
		p.instructions[p.curScope->chkStkIndex] |= stkLimit << 8;

		p.curScope->chkStkIndex = -1;
	}

	bool res = p.LeaveScope(1);

	if (!p.GetInline())
	{
		p.EndStackFrame();

		// handle static global exit functions
		if (fname.StartsWith("__exit$"))
		{
			LETHE_RET_FALSE(ValidateStaticInitSignature(p));
			p.EmitGlobalDestCall(startPC);
		}
	}

	return res;
}

bool AstFunc::CodeGenGlobalCtorStatic(CompiledProgram &p)
{
	// handle static global initializer functions
	auto *ftext = AstStaticCast<const AstText *>(nodes[IDX_NAME]);
	const auto &fname = ftext->text;

	if (fname.StartsWith("__init$"))
	{
		LETHE_RET_FALSE(ValidateStaticInitSignature(p));
		forwardRefs.Add(p.EmitForwardJump(OPC_CALL));
	}

	return Super::CodeGenGlobalCtor(p);
}

bool AstFunc::ValidateStaticInitSignature(CompiledProgram &p) const
{
	if (!nodes[IDX_ARGS]->nodes.IsEmpty())
		return p.Error(this, "argument count must be zero static initializer function");

	if (nodes[IDX_RET]->GetTypeDesc(p).GetTypeEnum() != DT_NONE)
		return p.Error(this, "return value must be void for static initializer function");

	return true;
}

void AstFunc::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstFunc *>(n);
	tmp->vtblIndex = vtblIndex;
	tmp->forwardRefs = forwardRefs;
	tmp->typeRef = typeRef;
}

}
