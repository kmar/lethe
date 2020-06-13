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

bool AstTypeClass::BeginCodegen(CompiledProgram &p)
{
	// no code should be generated for templates
	if (qualifiers & AST_Q_TEMPLATE)
		return true;

	LETHE_RET_FALSE(Super::BeginCodegen(p));

	// generate class name
	className = typeRef.ref->name;
	p.AddClassType(className, typeRef.ref);

	// generate ptrTypeRef now
	UniquePtr<DataType> dt = new DataType;
	dt->type = DT_STRONG_PTR;
	dt->align = dt->size = sizeof(void *);
	dt->elemType.ref = typeRef.ref;

	dt->name = String("^") + className;
	ptrTypeRef.ref = p.AddType(dt.Detach());
	ptrTypeRef.qualifiers |= AST_Q_SKIP_DTOR;

	dt = new DataType;
	dt->type = DT_WEAK_PTR;
	dt->align = dt->size = sizeof(void *);
	dt->elemType.ref = typeRef.ref;

	dt->name = String("w^") + className;
	weakPtrTypeRef.ref = p.AddType(dt.Detach());
	weakPtrTypeRef.qualifiers |= AST_Q_SKIP_DTOR;

	dt = new DataType;
	dt->type = DT_RAW_PTR;
	dt->align = dt->size = sizeof(void *);
	dt->elemType.ref = typeRef.ref;

	dt->name = String("r^") + className;
	rawPtrTypeRef.ref = p.AddType(dt.Detach());
	rawPtrTypeRef.qualifiers |= AST_Q_SKIP_DTOR;

	const_cast<DataType *>(rawPtrTypeRef.ref)->complementaryType = ptrTypeRef.ref;
	const_cast<DataType *>(rawPtrTypeRef.ref)->complementaryType2 = weakPtrTypeRef.ref;

	const_cast<DataType *>(weakPtrTypeRef.ref)->complementaryType = ptrTypeRef.ref;
	const_cast<DataType *>(weakPtrTypeRef.ref)->complementaryType2 = rawPtrTypeRef.ref;

	const_cast<DataType *>(ptrTypeRef.ref)->complementaryType = weakPtrTypeRef.ref;
	const_cast<DataType *>(ptrTypeRef.ref)->complementaryType2 = rawPtrTypeRef.ref;

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

	return VtblGenNestedClasses(p);
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
