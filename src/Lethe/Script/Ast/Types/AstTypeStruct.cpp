#include "AstTypeStruct.h"
#include "../AstText.h"
#include "../NamedScope.h"
#include <Lethe/Script/Ast/Function/AstFunc.h>
#include <Lethe/Script/Ast/Types/AstVarDeclList.h>
#include <Lethe/Script/TypeInfo/BaseObject.h>
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Compiler/Warnings.h>

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(AstTypeStruct)

// AstTypeStruct

bool AstTypeStruct::FoldConst(const CompiledProgram &p)
{
	bool res = false;

	if (qualifiers & AST_Q_TEMPLATE)
		return res;

	if (alignExpr && !alignExpr->IsConstant())
	{
		// FIXME: hack!
		AstNode temp(AST_NONE, location);
		temp.nodes.Add(alignExpr);
		alignExpr->parent = &temp;

		auto *old = alignExpr.Detach();

		res = old->FoldConst(p);

		if (res)
		{
			// align expr is dead now...
			alignExpr = temp.nodes[0];
			temp.nodes.Clear();
		}
		else
			alignExpr = old;

		alignExpr->parent = nullptr;
	}

	res |= Super::FoldConst(p);

	return res;
}

bool AstTypeStruct::BeginCodegen(CompiledProgram &p)
{
	DataTypeEnum typeEnum = type == AST_STRUCT ? DT_STRUCT : DT_CLASS;

	// try to create virtual type
	auto dt = new DataType;

	dt->type = typeEnum;

	// copy name if any
	if (!overrideName.IsEmpty())
		dt->name = overrideName;
	else if (nodes[IDX_NAME]->type == AST_IDENT)
		dt->name = AstStaticCast<const AstText *>(nodes[IDX_NAME])->GetQText(p);

	typeRef.qualifiers = qualifiers;
	typeRef.ref = p.AddType(dt);

	return Super::BeginCodegen(p);
}

AstFunc *AstTypeStruct::GetCustomCtor()
{
	for (Int i = 2; i < nodes.GetSize(); i++)
	{
		AstNode *n = nodes[i];

		if (n->type == AST_FUNC && (n->qualifiers & AST_Q_CTOR))
		{
			auto *res = AstStaticCast<AstFunc *>(n);

			if (AstFunc::IDX_BODY >= res->nodes.GetSize())
				return res;

			auto *fbody = res->nodes[AstFunc::IDX_BODY];
			return fbody->nodes.IsEmpty() ? nullptr : res;
		}
	}

	return nullptr;
}

AstFunc *AstTypeStruct::GetCustomDtor()
{
	for (Int i=2; i<nodes.GetSize(); i++)
	{
		AstNode *n = nodes[i];

		if (n->type == AST_FUNC && (n->qualifiers & AST_Q_DTOR))
			return AstStaticCast<AstFunc *>(n);
	}

	return nullptr;
}

AstNode::ResolveResult AstTypeStruct::Resolve(const ErrorHandler &e)
{
	if (qualifiers & AST_Q_TEMPLATE)
	{
		if (!(flags & AST_F_RESOLVED))
		{
			flags |= AST_F_RESOLVED;

			// force-resolve children
			AstIterator it(this);

			while (auto *n = it.Next())
				n->flags |= AST_F_RESOLVED;
		}

		return RES_OK;
	}

	// if we have base but base hasn't been resolved completely, we wait!
	if (nodes[IDX_BASE]->type != AST_BASE_NONE)
	{
		if (!nodes[IDX_BASE]->IsResolved())
			return nodes[IDX_BASE]->Resolve(e);
	}

	if (!ResolveNode(e))
	{
		e.Error(this, "cannot resolve struct node");
		return RES_ERROR;
	}

	if (alignExpr)
	{
		if (!alignExpr->ResolveNode(e))
			return RES_ERROR;
	}

	return Super::Resolve(e);
}

bool AstTypeStruct::ResolveNode(const ErrorHandler &)
{
	if (scopeRef->base)
		return true;

	// try to resolve base...
	AstNode *nbase = nodes[IDX_BASE];

	if (nbase->type != AST_BASE)
		return true;

	// FIXME: better?!
	AstNode *tbase = nbase->nodes[IDX_NAME]->target;

	if (tbase && tbase->type == AST_TYPEDEF)
		tbase = const_cast<AstNode *>(tbase->GetTypeNode());

	if (tbase && !scopeRef->SetBase(tbase->scopeRef))
	{
		// force to fail due to recursive typegen
		flags |= AST_F_LOCK;
	}

	return true;
}

bool AstTypeStruct::CodeGenComposite(CompiledProgram &p)
{
	LETHE_RET_FALSE(Super::CodeGenComposite(p));

	QDataType qdt = GetTypeDesc(p);

	// remove unused non-virtual non-native template functions
	// this is necessary if function body is invalid for a certain type,
	// like sort relying on relational ops but instantiated with a type that doesn't define them
	// as a bonus, we save time by not generating code for unused func bodies
	// also: you must be careful not to call these removed functions from C++!
	if (qualifiers & AST_Q_TEMPLATE_INSTANTIATED)
	{
		for (auto *n : nodes)
		{
			if (n->type != AST_FUNC)
				continue;

			if (n->qualifiers & (AST_Q_NATIVE|AST_Q_VIRTUAL|AST_Q_FUNC_REFERENCED))
				continue;

			// FIXME: make sure we don't remove operators; this is a hack at the moment
			if (scopeRef->operators.FindIndex(n) >= 0)
				continue;

			n->flags |= AST_F_SKIP_CGEN;
		}
	}

	// it's safe to remove virtual properties from members now
	// it's necessary for auto-indexed structs to work properly
	qdt.RemoveVirtualProps();

	if (!qdt.GetSize())
	{
		// note: hack for empty structs: must be at least 1 byte in size in order for custom dtors (scope guards)
		// to work. funny that in C++ empty structs also take 1 byte
		auto dt = const_cast<DataType *>(qdt.ref);
		dt->size = dt->align = 1;
	}

	// problem: vtblgen not generated!
	if (qdt.GetTypeEnum() == DT_CLASS)
		LETHE_RET_FALSE(VtblGen(p));

	if (qdt.HasDtor())
	{
		const DataType &dt = *qdt.ref;

		if (type == AST_STRUCT)
		{
			// check for static __copy function. if present, generate now and change funAssign to point to it
			auto *fcopy = scopeRef->FindSymbol(p.GetInternalFuncName(CompiledProgram::IFUNC_COPY));

			auto *fn = AstStaticCast<AstFunc *>(fcopy);

			if (fn)
			{
				// validate signature: static void __copy(ref this, ref this)
				auto *args = fn->GetArgs();

				if (fn->qualifiers & AST_Q_METHOD)
					return p.Error(fn, "__copy must be a static method");

				if (args->nodes.GetSize() != 2)
					return p.Error(fn, "__copy must take exactly two arguments (dst, src)");

				if (fn->GetResult()->GetTypeDesc(p).GetTypeEnum() != DT_NONE)
					return p.Error(fn->GetResult(), "__copy must return void");

				auto t0 = args->nodes[0]->GetTypeDesc(p);
				auto t1 = args->nodes[1]->GetTypeDesc(p);

				if (&t0.GetType() != &dt || &t1.GetType() != &dt)
					return p.Error(&t0.GetType() != &dt ? args->nodes[0] : args->nodes[1], "invalid argument type to __copy");

				if (!t0.IsReference() || !t1.IsReference())
					return p.Error(t0.IsReference() ? args->nodes[1] : args->nodes[0], "argument to __copy must be a reference");

				p.FlushOpt();
				dt.funAssign = p.instructions.GetSize();
				LETHE_RET_FALSE(fn->CodeGen(p));
				fn->flags |= AST_F_SKIP_CGEN;
			}
		}

		if (!dt.GenDtor(p))
			return p.Error(this, "failed to generate destructors");
	}

	if (qdt.GetTypeEnum() == DT_CLASS)
	{
		// fix vtblIndex[0]
		auto dst = p.cpool.data.GetData() + qdt.GetType().vtblOffset;
		*reinterpret_cast<IntPtr *>(dst) = qdt.GetType().funDtor;
	}

	if (qdt.HasCtor())
	{
		const DataType &dt = *qdt.ref;

		if (!dt.GenCtor(p))
			return p.Error(this, "failed to generate constructors");
	}

	return true;
}

bool AstTypeStruct::TypeGen(CompiledProgram &p)
{
	if (flags & AST_F_LOCK)
		return p.Error(this, "recursive type definition");

	postponeTypeGen.Clear();

	if (alignExpr)
	{
		if (alignExpr->type != AST_CONST_INT)
			return p.Error(alignExpr, "alignas requires constant integer expression");

		minAlign = alignExpr->num.i;

		if (!IsPowerOfTwo(minAlign))
			return p.Error(alignExpr, "alignment specifier must be power of two");

		alignExpr.Clear();
	}

	DataTypeEnum typeEnum = type == AST_STRUCT ? DT_STRUCT : DT_CLASS;

	// check if already generated
	if (flags & AST_F_TYPE_GEN)
		return true;

	flags |= AST_F_LOCK;

	// skip typegen for classes (will be postponed)
	for (Int i=0; i<nodes.GetSize(); i++)
	{
		auto *n = nodes[i];

		// templates...
		if (n->type == AST_TYPEDEF)
		{
			auto *ntn = n->GetTypeNode();

			if (ntn)
				n = const_cast<AstNode *>(ntn);
		}

		if (n->type != AST_CLASS)
			LETHE_RET_FALSE(n->TypeGen(p));
		else
			postponeTypeGen.Add(n);
	}

	// collect ctor/dtor flags from child nodes
	for (Int i=0; i<nodes.GetSize(); i++)
	{
		const auto n = nodes[i];
		qualifiers |= n->qualifiers & (AST_Q_CTOR | AST_Q_DTOR);
	}

	// try to create virtual type
	auto *dt = const_cast<DataType *>(typeRef.ref);

	dt->structQualifiers = qualifiers;

	dt->type = typeEnum;

	// copy name if any
	if (!overrideName.IsEmpty())
		dt->name = overrideName;
	else if (nodes[IDX_NAME]->type == AST_IDENT)
		dt->name = AstStaticCast<const AstText *>(nodes[IDX_NAME])->GetQText(p);

	Int ofs = 0;

	if (scopeRef->base)
	{
		AstNode *baseNode = scopeRef->base->node;

		if (!baseNode || baseNode->type != type)
			return p.Error(this, "invalid base type");

		if (baseNode->qualifiers & (AST_Q_PRIVATE|AST_Q_PROTECTED))
			p.Warning(this, "private/protected inheritance not supported", WARN_PRIV_PROT_INHERIT);

		if (!(baseNode->qualifiers & AST_Q_NATIVE) && (qualifiers & AST_Q_NATIVE))
		{
			// must allow explicit object as base for classes!
			if (baseNode->type != AST_CLASS || baseNode->GetTypeDesc(p).ref->name != "object")
				return p.Error(this, "native struct/class cannot derive from script struct/class");
		}

		qualifiers |= baseNode->qualifiers & AST_Q_NOCOPY;

		AstTypeStruct *sbase = AstStaticCast<AstTypeStruct *>(baseNode);

		if (!(sbase->flags & AST_F_TYPE_GEN))
			LETHE_RET_FALSE(sbase->TypeGen(p));

		dt->baseType = sbase->GetTypeDesc(p);
		dt->size = ofs = dt->baseType.ref->size;
		dt->align = dt->baseType.ref->align;

		if (dt->baseType.qualifiers & AST_Q_FINAL)
			return p.Error(nodes[IDX_BASE], "cannot derive from final base");

		nodes[IDX_BASE]->target = baseNode;

		qualifiers |= dt->baseType.qualifiers & (AST_Q_CTOR | AST_Q_DTOR);
	}
	else if (type == AST_CLASS)
	{
		dt->size = ofs = (Int)sizeof(BaseObject);
		dt->align = sizeof(void *);
	}

	// handling native classes here
	const NativeClass *ncls = nullptr;

	if ((qualifiers & AST_Q_NATIVE) && (type == AST_CLASS || type == AST_STRUCT))
	{
		const bool isClass = type == AST_CLASS;

		const auto &sname = dt->name;

		auto idx = isClass ? p.cpool.FindNativeClass(sname) : p.cpool.FindNativeStruct(sname);

		if (idx < 0)
			return p.Error(this, String::Printf("native %s not found: %s", isClass ? "class" : "struct", sname.Ansi()));

		ncls = &p.cpool.nClass[idx];

		if (isClass)
		{
			dt->size = Max<Int>(dt->size, ncls->size);
			dt->align = Max<Int>(dt->align, ncls->align);
		}

		dt->nativeCtor = ncls->ctor;
		dt->nativeDtor = ncls->dtor;

		if (dt->nativeCtor)
			qualifiers |= AST_Q_CTOR;

		if (dt->nativeDtor)
			qualifiers |= AST_Q_DTOR;
	}

	Int nativeMembers = 0;
	Int scriptMembers = 0;

	// we have to generate members now...
	for (Int i=2; i<nodes.GetSize(); i++)
	{
		AstNode *n = nodes[i];

		if (n->type != AST_VAR_DECL_LIST || (n->nodes[0]->qualifiers & (AST_Q_STATIC | AST_Q_CONSTEXPR)))
			continue;

		auto *mn = n->nodes[0];
		auto mtype = mn->GetTypeDesc(p);

		// copy editable qualifier to members
		mtype.qualifiers |= qualifiers & AST_Q_EDITABLE;

		if (mn->qualifiers & AST_Q_NATIVE)
			++nativeMembers;
		else
			++scriptMembers;

		if (nativeMembers && scriptMembers)
			return p.Error(n, "cannot mix native and non-native members");

		// this fixes some issues with sizeof
		if (!mtype.GetSize())
		{
			auto *tn = const_cast<AstNode *>(mn->GetTypeNode());

			if (tn && tn->TypeGen(p))
				mtype = mn->GetTypeDesc(p);
		}

		// note: we have to rebuild member types for array refs as well
		const bool skipTypegen = mtype.IsProperty() || mtype.GetTypeEnum() == DT_ARRAY_REF;

		if (skipTypegen)
		{
			qualifiers |= AST_Q_REBUILD_MEMBER_TYPES;
			mtype.qualifiers |= AST_Q_REBUILD_MEMBER_TYPES;
		}
		else
		{
			if (mtype.IsRecursive(typeRef.ref))
				return p.Error(mn, String::Printf("recursive member"));

			if (type == AST_CLASS && (qualifiers & AST_Q_STATE) != 0)
				return p.Error(mn, "state class cannot define new members");

			if (mtype.GetTypeEnum() == DT_NONE || !mtype.GetSize())
			{
				if (!mn->target)
					return p.Error(mn, "invalid member type");

				if (type == AST_CLASS && mn->target == this)
				{
					// avoid infinite recursion for pointers; we only need size/alignment here, types will be resolved later!
					mtype = QDataType::MakeConstType(p.elemTypes[DT_FUNC_PTR]);
					qualifiers |= AST_Q_REBUILD_MEMBER_TYPES;
					mtype.qualifiers |= AST_Q_REBUILD_MEMBER_TYPES;
				}
				else
				{
					LETHE_RET_FALSE(mn->target->TypeGen(p));
					mtype = mn->GetTypeDesc(p);
				}
			}
		}

		mtype.qualifiers &= ~AST_Q_SKIP_DTOR;

		// we can't possibly guard array references to be safe anyway, so we allow this
		//if (mtype.HasArrayRef())
		//	return p.Error(mn, "members cannot be array references");

		if (mtype.IsReference())
			return p.Error(mn, "members cannot be references");

		qualifiers |= mtype.qualifiers & AST_Q_NOCOPY;

		dt->align = Max(dt->align, mtype.GetAlign());

		for (Int j=1; j<n->nodes.GetSize(); j++)
		{
			AstNode *vn = n->nodes[j];

			if (vn->type != AST_VAR_DECL || vn->nodes[0]->type != AST_IDENT)
				continue;

			dt->members.Add(DataType::Member());
			DataType::Member &m = dt->members.Back();

			AstText *text = AstStaticCast<AstText *>(vn->nodes[0]);

			Int malign = mtype.GetAlign();
			Int msize  = mtype.GetSize();

			if (!msize)
			{
				// for empty structs as members, we imply 1 byte size
				if (mtype.IsStruct())
					msize = malign = 1;
				else
					return p.Error(vn, "illegal member type");
			}

			// virtual property?
			if (mn->qualifiers & AST_Q_PROPERTY)
				malign = msize = 0;

			Int mofs = 0;

			// handle native members...
			if (mn->qualifiers & AST_Q_NATIVE)
			{
				if (!ncls)
					return p.Error(vn, "native member without native struct/class");

				const auto ci = ncls->members.Find(text->text);

				if (ci == ncls->members.End())
					return p.Error(vn, String::Printf("native member not found: %s", text->text.Ansi()));

				mofs = ci->value;
				// native members destroyed by native dtor
				mtype.qualifiers |= AST_Q_SKIP_DTOR;
			}
			else if (msize)
			{
				// align offset
				ofs   += malign-1;
				ofs   = (ofs / malign) * malign;
				mofs = ofs;
				ofs += msize;
			}

			dt->size = Max(mofs + msize, dt->size);

			m.name   = text->text;
			m.node   = mn;
			m.attributes = AstStaticCast<AstVarDeclList *>(n)->attributes;
			m.type   = mtype;
			m.offset = mofs;

			vn->offset = mofs;
		}
	}

	typeRef.qualifiers = qualifiers;

	dt->align = Max<Int>(dt->align, minAlign);

	// pad-align size
	if (dt->align > 1)
		dt->size = (dt->size + dt->align - 1) / dt->align * dt->align;

	flags &= ~AST_F_LOCK;

	dt->ctorRef = GetCustomCtor();
	dt->funcRef = GetCustomDtor();
	dt->structScopeRef = scopeRef;

	if (type == AST_STRUCT && !dt->ctorRef && (typeRef.qualifiers & AST_Q_CTOR) && !(dt->baseType.qualifiers & AST_Q_CTOR))
	{
		// we may optimize the ctor flag away
		typeRef.qualifiers &= ~AST_Q_CTOR;
	}

	if (typeRef.HasCtor())
		typeRef.qualifiers |= AST_Q_CTOR;

	if (typeRef.HasDtor())
		typeRef.qualifiers |= AST_Q_DTOR;

	flags |= AST_F_TYPE_GEN;

	// error if class should be aligned to >16 bytes (ObjectHeap limitation)
	if (dt->type == DT_CLASS && dt->align > 16)
		return p.Error(this, "classes cannot be aligned to more than 16 bytes");

	if (dt->type == DT_STRUCT)
		for (auto *it : postponeTypeGen)
			if (!(it->flags & AST_F_LOCK))
				LETHE_RET_FALSE(it->TypeGen(p));

	dt->attributes = attributes;

	if ((qualifiers & AST_Q_NATIVE) && type == AST_STRUCT)
	{
		LETHE_ASSERT(ncls);

		const auto &nc = *ncls;

		if (nativeMembers || !(dt->size | dt->align))
		{
			dt->size = Max<Int>(dt->size, ncls->size);
			dt->align = Max<Int>(dt->align, ncls->align);
		}

		if (dt->size != nc.size || dt->align != nc.align)
			return p.Error(this, String::Printf("native struct size/alignment mismatch: %s (script: %d/%d native: %d/%d)",
				nc.name.Ansi(), dt->size, dt->align, nc.size, nc.align
			));
	}

	return true;
}

bool AstTypeStruct::GetTemplateTypeText(StringBuilder &sb) const
{
	LETHE_RET_FALSE(!(qualifiers & AST_Q_TEMPLATE));

	if (type == AST_STRUCT)
		sb.AppendFormat("struct ");

	return nodes[IDX_NAME]->GetTemplateTypeText(sb);
}

bool AstTypeStruct::IsTemplateArg(const StringRef &sr) const
{
	return FindTemplateArg(sr) != nullptr;
}

const AstTypeStruct::TemplateArg *AstTypeStruct::FindTemplateArg(const StringRef &sr) const
{
	for (auto &&it : templateArgs)
		if (it.name == sr)
			return &it;

	return nullptr;
}

void AstTypeStruct::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstTypeStruct *>(n);
	tmp->templateArgs = templateArgs;
	tmp->overrideName = overrideName;

	if (alignExpr)
		tmp->alignExpr = alignExpr->Clone();

	tmp->minAlign = minAlign;
}


}
