#include "AstTypeClass.h"
#include "../AstText.h"
#include "../NamedScope.h"
#include <Lethe/Script/Ast/Function/AstFunc.h>
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Compiler/Warnings.h>

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(AstTypeClass)

// AstTypeClass

// vtblIndex starts at 1; 0 is reserved for dtor
AstTypeClass::AstTypeClass(const TokenLocation &nloc)
	: Super(nloc)
	, vtblIndex(1)
	, vtblOffset(-1)
{
	type = AST_CLASS;
}

QDataType AstTypeClass::GetTypeDescPtr(DataTypeEnum dte) const
{
	return dte == DT_RAW_PTR ? rawPtrTypeRef : dte == DT_WEAK_PTR ? weakPtrTypeRef : ptrTypeRef;
}

Name AstTypeClass::GetName() const
{
	return className;
}

void AstTypeClass::GenPtrTypes(CompiledProgram &p, Name cname, const DataType *ntype, QDataType &ptype, QDataType &wptype, QDataType &rptype)
{
	// generate ptrTypeRef now
	UniquePtr<DataType> dt = new DataType;
	dt->type = DT_STRONG_PTR;
	dt->align = dt->size = sizeof(void *);
	dt->elemType.ref = ntype;

	dt->name = String("^") + cname;
	ptype.ref = p.AddType(dt.Detach());
	ptype.qualifiers |= AST_Q_SKIP_DTOR;

	dt = new DataType;
	dt->type = DT_WEAK_PTR;
	dt->align = dt->size = sizeof(void *);
	dt->elemType.ref = ntype;

	dt->name = String("w^") + cname;
	wptype.ref = p.AddType(dt.Detach());
	wptype.qualifiers |= AST_Q_SKIP_DTOR;

	dt = new DataType;
	dt->type = DT_RAW_PTR;
	dt->align = dt->size = sizeof(void *);
	dt->elemType.ref = ntype;

	dt->name = String("r^") + cname;
	rptype.ref = p.AddType(dt.Detach());
	rptype.qualifiers |= AST_Q_SKIP_DTOR;

	const_cast<DataType *>(rptype.ref)->complementaryType = ptype.ref;
	const_cast<DataType *>(rptype.ref)->complementaryType2 = wptype.ref;

	const_cast<DataType *>(wptype.ref)->complementaryType = ptype.ref;
	const_cast<DataType *>(wptype.ref)->complementaryType2 = rptype.ref;

	const_cast<DataType *>(ptype.ref)->complementaryType = wptype.ref;
	const_cast<DataType *>(ptype.ref)->complementaryType2 = rptype.ref;
}

bool AstTypeClass::BeginCodegen(CompiledProgram &p)
{
	// no code should be generated for templates
	if (qualifiers & AST_Q_TEMPLATE)
		return true;

	LETHE_RET_FALSE(Super::BeginCodegen(p));

	// generate class name
	className = typeRef.ref->name;
	p.AddClassType(className, typeRef.ref);

	GenPtrTypes(p, className, typeRef.ref, ptrTypeRef, weakPtrTypeRef, rawPtrTypeRef);
	return true;
}

bool AstTypeClass::TypeGen(CompiledProgram &p)
{
	// check if already generated
	if (flags & AST_F_TYPE_GEN)
		return true;

	LETHE_RET_FALSE(Super::TypeGen(p));

	typeRef.ref->GenBaseChain();

	if (qualifiers & AST_Q_REBUILD_MEMBER_TYPES)
	{
		// late-fix member types!
		auto dtRef = const_cast<DataType *>(typeRef.ref);

		for (Int i=0; i<dtRef->members.GetSize(); i++)
		{
			auto &m = dtRef->members[i];

			if (m.type.qualifiers & AST_Q_REBUILD_MEMBER_TYPES)
			{
				m.type = m.node->GetTypeDesc(p);
				m.type.qualifiers &= ~AST_Q_SKIP_DTOR;
				typeRef.qualifiers = qualifiers |= m.type.qualifiers & AST_Q_NOCOPY;
			}
		}
	}

	// handle base
	AstNode *base = nodes[1]->target;
	AstTypeClass *baseClass = 0;

	if (base && base->type == AST_CLASS)
	{
		baseClass = AstStaticCast<AstTypeClass *>(base);

		if (baseClass->typeRef.GetTypeEnum() == DT_NONE)
			LETHE_RET_FALSE(baseClass->TypeGen(p));

		vtblIndex = baseClass->vtblIndex;
	}

	// generate vtbl indices
	// now go through all non-native non-final non-static methods
	for (Int i=2; i<nodes.GetSize(); i++)
	{
		AstNode *n = nodes[i];

		if (n->type != AST_FUNC || !(n->qualifiers & AST_Q_METHOD))
			continue;

		if (n->qualifiers & (AST_Q_FINAL | AST_Q_STATIC | AST_Q_CTOR))
			continue;

		// search for method with same name
		const String &mname = AstStaticCast<AstText *>(n->nodes[1])->text;

		AstNode *mnode = 0;

		if (baseClass)
			mnode = baseClass->scopeRef->FindSymbol(mname, true);

		if (mnode && mnode->type != AST_FUNC)
			mnode = 0;

		Int vidx = -1;

		if (mnode)
		{
			// copy state for state method
			if (mnode->qualifiers & AST_Q_STATE)
				n->qualifiers |= AST_Q_STATE;

			// examine qualifiers
			if (mnode->qualifiers & AST_Q_FINAL)
				return p.Error(n, "method already final");

			if (mnode->qualifiers & AST_Q_STATIC)
				return p.Error(n, "method already static");

			// make sure it's accessible
			if (mnode->qualifiers & AST_Q_PRIVATE)
				return p.Error(n, "cannot override private method");

			// validate signature for virtual methods
			LETHE_RET_FALSE(AstStaticCast<const AstFunc *>(n)->ValidateSignature(*AstStaticCast<const AstFunc *>(mnode), p));

			// TODO: what about native (bridge?)
			vidx = AstStaticCast<AstFunc *>(mnode)->vtblIndex;

			if ((n->qualifiers & (AST_Q_VIRTUAL | AST_Q_OVERRIDE)) == AST_Q_VIRTUAL)
				p.Warning(n, "missing override specifier", WARN_MISSING_OVERRIDE);
		}

		if (n->qualifiers & AST_Q_OVERRIDE)
		{
			if (!(n->qualifiers & AST_Q_VIRTUAL) || !mnode)
				return p.Error(n, "no virtual override");
		}

		if (vidx < 0 && type == AST_CLASS && (qualifiers & AST_Q_STATE) != 0)
			return p.Error(n, "state classes cannot add new virtual methods (did you mean final?)");

		AstStaticCast<AstFunc *>(n)->vtblIndex = vidx < 0 ? vtblIndex++ : vidx;
	}

	for (auto *it : postponeTypeGen)
		if (!(it->flags & AST_F_LOCK))
			LETHE_RET_FALSE(it->TypeGen(p));

	return true;
}

bool AstTypeClass::VtblGenNestedClasses(CompiledProgram &p)
{
	// VtblGen nested classes
	for (Int i=2; i<nodes.GetSize(); i++)
	{
		AstNode *n = nodes[i];

		if (n->type == AST_CLASS)
			LETHE_RET_FALSE(n->VtblGen(p));
	}

	return true;
}

bool AstTypeClass::VtblGen(CompiledProgram &p)
{
	if (qualifiers & AST_Q_TEMPLATE)
		return true;

	// note: no need to call Super::VtblGen here because we don't support nested classes anyway
	QDataType qdtJit = QDataType::MakeConstType(p.elemTypes[DT_STRONG_PTR]);

	AstNode *base = nodes[1]->target;
	AstTypeClass *baseClass = 0;

	if (base && base->type == AST_CLASS)
		baseClass = AstStaticCast<AstTypeClass *>(base);

	// gen parent vtbl first
	if (baseClass && baseClass->vtblOffset < 0)
		LETHE_RET_FALSE(baseClass->VtblGen(p));

	LETHE_ASSERT(!baseClass || vtblIndex >= baseClass->vtblIndex);

	if (vtblOffset < 0)
	{
		// reserve space for engine refptr (vtbl index -3) and script instance deleter (vtbl index-2)
		p.cpool.AllocGlobal(qdtJit);
		p.cpool.AllocGlobal(qdtJit);
		// reserve space for classType (vtbl index -1)
		Int clsOfs = p.cpool.AllocGlobal(qdtJit);

		// set refptr to class type
		*reinterpret_cast<const DataType **>(p.cpool.data.GetData() + clsOfs) = typeRef.ref;

		// setup vtbl (engine/deleter)
		p.SetupVtbl(clsOfs);

		vtblOffset = p.cpool.AllocGlobal(qdtJit);
		auto clsdt = const_cast<DataType *>(typeRef.ref);
		clsdt->vtblOffset = vtblOffset;
		clsdt->vtblSize = vtblIndex;

		for (Int j=1; j<vtblIndex; j++)
			p.cpool.AllocGlobal(qdtJit);

		p.AddVtbl(vtblOffset, vtblIndex);
	}

	IntPtr *dst = reinterpret_cast<IntPtr *>(p.cpool.data.GetData() + vtblOffset);

	// copy base vtbl
	if (baseClass)
	{
		LETHE_ASSERT(baseClass->vtblOffset >= 0);
		const IntPtr *src = reinterpret_cast<IntPtr *>(p.cpool.data.GetData() + baseClass->vtblOffset);

		virtualMethods = baseClass->virtualMethods;

		for (Int i=0; i<baseClass->vtblIndex; i++)
			dst[i] = src[i];
	}

	// vtbl index 0 is always dtor
	Int fdtor = GetTypeDesc(p).GetType().funDtor;
	dst[0] = fdtor;

	for (Int i=2; i<nodes.GetSize(); i++)
	{
		AstNode *n = nodes[i];

		if (n->type != AST_FUNC || !(n->qualifiers & AST_Q_METHOD))
			continue;

		if (n->qualifiers & (AST_Q_FINAL | AST_Q_STATIC | AST_Q_CTOR))
			continue;

		const auto *func = AstStaticCast<const AstFunc *>(n);

		const auto &mname = AstStaticCast<AstText *>(func->nodes[AstFunc::IDX_NAME])->text;

		Int pc = n->offset;

		if (ignores.FindIndex(mname) >= 0)
			return p.Error(n, "method already tagged as ignored");

		virtualMethods[mname] = func->vtblIndex;

		// since we call vtblgen multiple times it's ok if pc is not known yet
		dst[func->vtblIndex] = pc;
	}

	if (!ignores.IsEmpty())
	{
		// apply "Ignores"
		for (auto &&it : ignores)
		{
			auto ci = virtualMethods.Find(it);

			if (ci == virtualMethods.End())
				return p.Error(this, String::Printf("ignored method %s not found or final", it.Ansi()));

			// PC 1 = empty function
			dst[ci->value] = 1;
		}
	}

	return VtblGenNestedClasses(p) && InjectBaseStates(p);
}

bool AstTypeClass::CodeGenComposite(CompiledProgram &p)
{
	// no code should be generated for templates
	if (qualifiers & AST_Q_TEMPLATE)
		return true;

	LETHE_RET_FALSE(Super::CodeGenComposite(p));

	// setup lookup map for nested state classes

	for (auto *it : nodes)
	{
		if (it->type != AST_CLASS || !(it->qualifiers & AST_Q_STATE))
			continue;

		auto sclass = it->GetTypeDesc(p);
		auto cname = sclass.GetType().name;

		// we need to strip '::' first
		auto start = cname.ReverseFind(':');

		if (start < 0)
			continue;

		++start;
		auto localName = Name(cname.Ansi() + start);
		p.stateToLocalNameMap[cname] = localName;
		p.fixupStateMap[CompiledProgram::PackNames(typeRef.ref->name, localName)] = cname;
	}

	// generate pointer dtors...
	return ptrTypeRef.ref->GenDtor(p) && weakPtrTypeRef.ref->GenDtor(p);
}

bool AstTypeClass::InjectBaseStates(CompiledProgram &p)
{
	if ((qualifiers & AST_Q_STATE) || !scopeRef->base)
		return true;

	// at this point we should be able to auto-inherit nested state classes of base class (that haven't been defined here)
	StackArray<AstNode *, 64> baseStates;

	for (auto &&it : scopeRef->base->namedScopes)
	{
		auto *n = it.value->node;

		if (!n || n->type != AST_CLASS || !(n->qualifiers & AST_Q_STATE) || it.value->base != scopeRef->base)
			continue;

		if (scopeRef->namedScopes.FindIndex(it.key) < 0)
			baseStates.Add(n);
	}

	if (baseStates.IsEmpty())
		return true;

	// okay, we have work to do!
	auto *baseClass = scopeRef->base->node;
	auto baseType = baseClass->GetTypeDesc(p);
	auto thisType = GetTypeDesc(p);

	auto ptrType = QDataType::MakeConstType(p.elemTypes[DT_STRONG_PTR]);

	// PROBLEM: vtblgen is called multiple times!

	// vtbl:
	// -3 = engine ptr
	// -2 = script instance deleter
	// -1 = class type ptr
	// 0  = dtor
	// 1+ = methods

	for (auto *it : baseStates)
	{
		auto stateType = it->GetTypeDesc(p);
		LETHE_ASSERT(stateType.GetType().vtblSize == baseType.GetType().vtblSize);

		// generate name...
		auto stateName = stateType.GetType().name;
		Int firstColon = stateName.ReverseFind(':');

		if (firstColon < 0)
			return p.Error(this, "invalid base state name");

		StringRef sr(stateName.Ansi() + firstColon+1, stateName.GetLength() - firstColon-1);

		const auto &thisName = thisType.GetType().name;
		StringBuilder sb(thisName.Ansi());
		sb += "::";
		sb += sr;

		String newName = sb.Get();
		Name localName = sr.Ansi();
		Name newClsName = newName;

		DataType *ntype = const_cast<DataType *>(p.FindClass(newClsName));

		if (ntype && ntype->type != DT_CLASS)
			continue;

		UniquePtr<DataType> ntypePtr;

		if (!ntype)
		{
			ntypePtr = new DataType;
			ntype = ntypePtr;
			ntype->type = DT_CLASS;
		}

		if (ntypePtr)
		{
			// [-3] = engine refptr
			p.cpool.AllocGlobal(ptrType);
			// [-2] = deleter
			p.cpool.AllocGlobal(ptrType);
			// [-1] = class type
			auto classOfs = p.cpool.AllocGlobal(ptrType);

			// set refptr to class type
			*reinterpret_cast<const DataType **>(p.cpool.data.GetData() + classOfs) = ntype;

			p.SetupVtbl(classOfs);
		}

		// [0] = dtor
		auto vtblOfs = ntypePtr ? -1 : ntype->vtblOffset;

		for (Int i=0; i<thisType.GetType().vtblSize; i++)
		{
			auto vtblValueThis  = *reinterpret_cast<const IntPtr *>(&p.cpool.data[thisType.GetType().vtblOffset + i*sizeof(IntPtr)]);
			IntPtr value = vtblValueThis;

			// keep dtor
			if (i > 0 && i < stateType.GetType().vtblSize)
			{
				// we want to copy vtbl values overriden by state (in base), otherwise keep this vtbl value
				auto vtblValueBase  = *reinterpret_cast<const IntPtr *>(&p.cpool.data[baseType.GetType().vtblOffset + i*sizeof(IntPtr)]);
				auto vtblValueState = *reinterpret_cast<const IntPtr *>(&p.cpool.data[stateType.GetType().vtblOffset + i*sizeof(IntPtr)]);

				if (vtblValueBase != vtblValueState)
					value = vtblValueState;
			}

			auto entryOfs = ntypePtr ? p.cpool.AllocGlobal(ptrType) : Int(vtblOfs + i*sizeof(IntPtr));

			if (vtblOfs < 0)
				vtblOfs = entryOfs;

			*reinterpret_cast<IntPtr *>(&p.cpool.data[entryOfs]) = value;
		}

		ntype->funCtor = thisType.GetType().funCtor;
		ntype->funDtor = thisType.GetType().funDtor;

		if (!ntypePtr)
			continue;

		ntype->align = thisType.GetType().align;
		ntype->size  = thisType.GetType().size;
		ntype->vtblOffset = vtblOfs;
		ntype->vtblSize = thisType.GetType().vtblSize;
		ntype->structQualifiers = stateType.GetType().structQualifiers;
		ntype->baseType = thisType;
		ntype->name = newName;
		ntype->className = newClsName;

		if (p.typeHash.FindIndex(newName) >= 0)
			return p.Error(this, "cannot inherit base state: type already defined");

		p.AddVtbl(ntype->vtblOffset, ntype->vtblSize);

		p.stateToLocalNameMap[newName] = localName;
		p.fixupStateMap[CompiledProgram::PackNames(thisType.GetType().name, localName)] = newClsName;

		p.AddClassType(newClsName, ntype);
		p.AddType(ntypePtr.Detach());

		QDataType ptype, wptype, rptype;
		GenPtrTypes(p, newClsName, ntype, ptype, wptype, rptype);

		ntype->GenBaseChain();
	}

	return true;
}

void AstTypeClass::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstTypeClass *>(n);
	tmp->virtualMethods = virtualMethods;
	tmp->ignores = ignores;
	tmp->ptrTypeRef = ptrTypeRef;
	tmp->weakPtrTypeRef = weakPtrTypeRef;
	tmp->className = className;
	tmp->vtblIndex = vtblIndex;
	tmp->vtblOffset = vtblOffset;
}


}
