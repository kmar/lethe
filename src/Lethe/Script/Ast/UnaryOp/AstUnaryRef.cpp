#include "AstUnaryRef.h"
#include "../AstSymbol.h"
#include "../NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

LETHE_AST_BUCKET_ALLOC_DEF(AstUnaryRef)

// AstUnaryRef

AstUnaryRef::~AstUnaryRef()
{
	// don't allow double free of shared type node
	if (aref)
		aref->nodes.Clear();
}

bool AstUnaryRef::TypeGen(CompiledProgram &p)
{
	(void)GetTypeNode();
	return GetArrayRefNode()->TypeGen(p);
}

const AstNode *AstUnaryRef::GetTypeNode() const
{
	auto *sym = nodes[IDX_REF]->FindVarSymbolNode();
	auto *tn = nodes[IDX_REF]->GetTypeNode();

	if (!tn)
		return nullptr;

	bool isStatic = (tn->qualifiers & AST_Q_STATIC) != 0;

	if (!isStatic && sym && sym->symScopeRef && sym->symScopeRef->IsComposite() &&
		sym->symScopeRef->IsBaseOf(scopeRef->FindThis()) && scopeRef->IsConstMethod())
	{
		// must make const
		nodes[IDX_REF]->qualifiers |= AST_Q_CONST;
	}

	return GetArrayRefNode();
}

AstNode::ResolveResult AstUnaryRef::Resolve(const ErrorHandler &e)
{
	auto res = Super::Resolve(e);

	if ((flags & AST_F_RESOLVED) && aref)
		aref->flags |= AST_F_RESOLVED;

	return res;
}

AstNode *AstUnaryRef::GetArrayRefNode() const
{
	if (!aref)
	{
		aref = new AstTypeArrayRef(location);
		aref->nodes.Add(nodes[IDX_REF]);
	}

	return aref;
}

bool AstUnaryRef::CodeGen(CompiledProgram &p)
{
	p.EmitIntConst(1);
	p.PushStackType(QDataType::MakeConstType(p.elemTypes[DT_INT]));
	LETHE_RET_FALSE(nodes[IDX_REF]->CodeGenRef(p, true));
	p.PopStackType(true);
	p.PopStackType(true);

	auto resType = GetTypeDesc(p);

	if (nodes[IDX_REF]->type == AST_THIS && resType.GetType().elemType.IsPointer())
		return p.Error(nodes[IDX_REF], "cannot construct ref from this inside a class");

	p.PushStackType(resType);
	return true;
}

QDataType AstUnaryRef::GetTypeDesc(const CompiledProgram &p) const
{
	return GetArrayRefNode()->GetTypeDesc(p);
}

bool AstUnaryRef::FoldConst(const CompiledProgram &p)
{
	bool res = false;

	for (auto *it : nodes[IDX_REF]->nodes)
		res |= it->FoldConst(p);

	return res;
}

void AstUnaryRef::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstUnaryRef *>(n);

	if (aref)
		tmp->GetArrayRefNode();
}


}
