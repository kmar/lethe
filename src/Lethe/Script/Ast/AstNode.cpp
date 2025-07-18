#include <Lethe/Core/Io/Stream.h>
#include <Lethe/Core/String/Name.h>
#include "AstNode.h"
#include "AstText.h"
#include "AstSymbol.h"
#include <Lethe/Script/Ast/Types/AstTypeStruct.h>
#include <Lethe/Script/Ast/Types/AstTypeInt.h>

#include "Constants/AstConstInt.h"
#include "Constants/AstConstUInt.h"
#include "Constants/AstConstLong.h"
#include "Constants/AstConstULong.h"
#include "Constants/AstConstFloat.h"
#include "Constants/AstConstDouble.h"
#include "Constants/AstConstName.h"
#include "Constants/AstConstString.h"
#include "Constants/AstEnumItem.h"

#include "Types/AstTypeEnum.h"

#include "Function/AstFunc.h"
#include "Function/AstCall.h"
#include "CodeGenTables.h"

#include "NamedScope.h"

#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Stack.h>
#include <Lethe/Script/Vm/Builtin.h>

namespace lethe
{

// allocators
LETHE_AST_BUCKET_ALLOC_DEF(AstNode)

// must be in sync with enum!
static const char *AST_TYPE_NAMES[] =
{
	"AST_NONE",
	"AST_CONST_BOOL",
	"AST_CONST_CHAR",
	"AST_CONST_INT",
	"AST_CONST_UINT",
	"AST_CONST_LONG",
	"AST_CONST_ULONG",
	"AST_CONST_FLOAT",
	"AST_CONST_DOUBLE",
	"AST_CONST_NULL",
	"AST_NAME_LIT",
	"AST_STRING_LIT",
	"AST_THIS",
	"AST_SUPER",
	// identifier
	"AST_IDENT",
	// scope resolution
	"AST_OP_SCOPE_RES",
	// dot operator
	"AST_OP_DOT",
	// ast subscript (array)
	"AST_OP_SUBSCRIPT",
	// ternary ? :
	"AST_OP_TERNARY",
	"AST_UOP_PLUS",
	"AST_UOP_MINUS",
	"AST_UOP_NOT",
	"AST_UOP_LNOT",
	"AST_UOP_PREINC",
	"AST_UOP_PREDEC",
	"AST_UOP_POSTINC",
	"AST_UOP_POSTDEC",
	"AST_UOP_REF",
	"AST_OP_MUL",
	"AST_OP_DIV",
	"AST_OP_MOD",
	"AST_OP_ADD",
	"AST_OP_SUB",
	"AST_OP_SHL",
	"AST_OP_SHR",
	"AST_OP_LT",
	"AST_OP_LEQ",
	"AST_OP_GT",
	"AST_OP_GEQ",
	"AST_OP_EQ",
	"AST_OP_NEQ",
	"AST_OP_AND",
	"AST_OP_XOR",
	"AST_OP_OR",
	"AST_OP_LAND",
	"AST_OP_LOR",
	"AST_OP_THROW",
	"AST_OP_ASSIGN",
	"AST_OP_ADD_ASSIGN",
	"AST_OP_SUB_ASSIGN",
	"AST_OP_MUL_ASSIGN",
	"AST_OP_DIV_ASSIGN",
	"AST_OP_MOD_ASSIGN",
	"AST_OP_SHL_ASSIGN",
	"AST_OP_SHR_ASSIGN",
	"AST_OP_AND_ASSIGN",
	"AST_OP_XOR_ASSIGN",
	"AST_OP_OR_ASSIGN",
	"AST_OP_SWAP",
	"AST_OP_SWAP_NULL",
	"AST_OP_COMMA",
	"AST_NEW",
	"AST_CAST_OR_CALL",
	"AST_CAST",
	"AST_SIZEOF",
	"AST_OFFSETOF",
	"AST_ALIGNOF",
	"AST_TYPEID",
	"AST_CALL",
	"AST_TYPE_VOID",
	"AST_TYPE_BOOL",
	"AST_TYPE_BYTE",
	"AST_TYPE_SBYTE",
	"AST_TYPE_SHORT",
	"AST_TYPE_USHORT",
	"AST_TYPE_CHAR",
	"AST_TYPE_INT",
	"AST_TYPE_UINT",
	"AST_TYPE_LONG",
	"AST_TYPE_ULONG",
	"AST_TYPE_FLOAT",
	"AST_TYPE_DOUBLE",
	"AST_TYPE_NAME",
	"AST_TYPE_STRING",
	"AST_TYPE_AUTO",
	"AST_TYPE_ARRAY",
	"AST_TYPE_DYNAMIC_ARRAY",
	"AST_TYPE_ARRAY_REF",

	"AST_ARG_LIST",
	"AST_ARG",
	"AST_ARG_ELLIPSIS",
	"AST_FUNC",
	"AST_BLOCK",
	"AST_PROGRAM",
	"AST_VAR_DECL_LIST",
	"AST_VAR_DECL",
	"AST_EXPR",
	"AST_RETURN",
	"AST_RETURN_VALUE",
	"AST_BREAK",
	"AST_CONTINUE",
	"AST_IF",
	"AST_WHILE",
	"AST_DO",
	"AST_FOR",
	"AST_FOR_RANGE",
	"AST_SWITCH",
	"AST_SWITCH_BODY",
	"AST_CASE",
	"AST_CASE_DEFAULT",
	"AST_EMPTY",
	"AST_CLASS",
	"AST_STRUCT",
	"AST_ENUM",
	"AST_ENUM_ITEM",
	"AST_TYPEDEF",
	"AST_BASE",
	"AST_BASE_NONE",
	"AST_PROGRAM_LIST",
	"AST_FUNC_BODY",
	"AST_INITIALIZER_LIST",
	"AST_TYPE_FUNC_PTR",
	"AST_TYPE_DELEGATE",
	"AST_NAMESPACE",
	"AST_IMPORT",
	"AST_DEFER",
	"AST_DEFAULT_INIT",
	"AST_STRUCT_LITERAL",
	"AST_NPROP",
	"AST_NPROP_METHOD",
	"AST_LABEL",
	"AST_GOTO",
	"AST_STATIC_ASSERT",
	"AST_BITSIZEOF",
	"AST_BITOFFSETOF",
	nullptr
};

// AstIterator

AstIterator::AstIterator(AstNode *node)
{
	if (node)
		queue.AddBack(node);
}

AstNode *AstIterator::Next(Int exclude)
{
	AstNode *res;

	do
	{
		LETHE_RET_FALSE(!queue.IsEmpty());
		res = queue.Front();
		queue.PopFront();

		if (res->type != exclude)
			for (Int i=0; i<res->nodes.GetSize(); i++)
				queue.AddBack(res->nodes[i]);
	}
	while (!Accept(res));

	return res;
}

bool AstIterator::Accept(AstNode *node) const
{
	(void)node;
	return true;
}

AstConstIterator::AstConstIterator(const AstNode *node)
{
	if (node)
		queue.AddBack(node);
}

const AstNode *AstConstIterator::Next()
{
	const AstNode *res;

	do
	{
		LETHE_RET_FALSE(!queue.IsEmpty());
		res = queue.Front();
		queue.PopFront();

		for (Int i=0; i<res->nodes.GetSize(); i++)
			queue.AddBack(res->nodes[i]);
	}
	while (!Accept(res));

	return res;
}

bool AstConstIterator::Accept(const AstNode *node) const
{
	(void)node;
	return true;
}

// AstNode

constexpr Int AstNode::UNROLL_MAX_COUNT;
constexpr Int AstNode::UNROLL_MIN_WEIGHT;

AstNode::AstNode(AstNodeType ntype, const TokenLocation &nloc)
	: parent(nullptr)
	, target(nullptr)
	, type(ntype)
	, flags(0)
	, offset(-1)
	, scopeRef(nullptr)
	, symScopeRef(nullptr)
	, qualifiers(0)
	, location(nloc)
	, cachedIndex(-1)
{
	num.d = 0;
}

AstNode::~AstNode()
{
	ClearNodes();
}

DataType *AstNode::GenFuncType(AstNode *fref, CompiledProgram &p, AstNode *resType, const Array<AstNode *> &args, bool isDelegate)
{
	DataType *dt = new DataType;
	dt->funcRef = fref;
	dt->type = isDelegate ? DT_DELEGATE : DT_FUNC_PTR;
	dt->size = dt->align = sizeof(IntPtr);

	if (isDelegate)
		dt->size *= 2;

	dt->elemType = resType->GetTypeDesc(p);

	auto argCount = args.GetSize();
	dt->argTypes.Resize(argCount);

	auto resName = dt->elemType.GetName();

	if (isDelegate)
		resName += " delegate(";
	else
	{
		if (!fref)
			resName += '(';
		else
			resName += " function(";
	}

	for (Int i = 0; i<argCount; i++)
	{
		dt->argTypes[i] = args[i]->GetTypeDesc(p);

		LETHE_ASSERT(args[i]->type == AST_ARG_ELLIPSIS || dt->argTypes[i].GetTypeEnum() != DT_NONE);

		auto argName = dt->argTypes[i].GetName();

		if (args[i]->type == AST_ARG_ELLIPSIS)
			resName += "...";
		else
			resName += argName;

		if (i + 1 < argCount)
			resName += ", ";
	}

	resName += ')';

	dt->name = resName;

	return dt;
}

bool AstNode::BakeGlobalData(AstNode *n, QDataType qdt, Int ofs, CompiledProgram &p)
{
	Byte *gdata = p.cpool.data.GetData();

	switch (qdt.GetTypeEnumUnderlying())
	{
	case DT_NULL:
		// no need to zero-init
		break;

	case DT_BOOL:
	{
		bool b = n->num.i != 0;
		MemCpy(gdata + ofs, &b, sizeof(bool));
		break;
	}

	case DT_BYTE:
	case DT_SBYTE:
	{
		Byte b = (Byte)n->num.i;
		gdata[ofs] = b;
		break;
	}

	case DT_SHORT:
	case DT_USHORT:
	{
		UShort b = (UShort)n->num.ui;
		MemCpy(gdata + ofs, &b, sizeof(b));
		break;
	}

	case DT_FLOAT:
		MemCpy(gdata + ofs, &n->num.f, sizeof(Float));
		break;

	case DT_DOUBLE:
		MemCpy(gdata + ofs, &n->num.d, sizeof(Double));
		break;

	case DT_INT:
	case DT_UINT:
	case DT_ENUM:
	case DT_CHAR:
		MemCpy(gdata + ofs, &n->num.ui, sizeof(UInt));
		break;

	case DT_LONG:
	case DT_ULONG:
	case DT_NAME:
		MemCpy(gdata + ofs, &n->num.ul, sizeof(ULong));
		break;

	case DT_STRING:
	{
		auto sptr = reinterpret_cast<String *>(gdata + ofs);
		// assume null
		*sptr = AstStaticCast<AstText *>(n)->text;
		p.cpool.AddGlobalBakedString(ofs);
		break;
	}

	default:;
		return p.Error(n, "invalid type for direct global init");
	}

	return true;
}

bool AstNode::ShouldPop() const
{
	LETHE_RET_FALSE(parent);

	switch(parent->type)
	{
	case AST_EXPR:
		return true;
	case AST_OP_COMMA:
		return parent->parent ? parent->parent->type == AST_OP_COMMA : false;
	default:;
	}

	return false;
}

void AstNode::ClearNodes()
{
	for (Int i = 0; i < nodes.GetSize(); i++)
	{
		AstNode *n = nodes[i];

		if (n)
			n->parent = nullptr;

		delete n;
	}

	nodes.Clear();
}

AstNode *AstNode::GetResolveTarget() const
{
	return target;
}

void AstNode::LoadIfVarDecl(CompiledProgram &)
{
}

// get tree depth
Int AstNode::GetDepth() const
{
	Int res = 1;

	for (auto &&n : nodes)
		res = Max(res, 1+n->GetDepth());

	return res;
}

const AstNode *AstNode::FindDefinition(Int col, Int line, const String &filename) const
{
	if (location.line == line && location.file == filename)
	{
		// potential match...
		if (type == AST_TYPE_AUTO)
		{
			if (col >= location.column && col < location.column + 4)
				return GetTypeNode();
		}

		if (type == AST_IDENT)
		{
			const AstSymbol *sym = AstStaticCast<const AstSymbol *>(this);
			const String &text = sym->text;

			if (col >= location.column && col < location.column+text.GetLength())
				return sym->target ? AstStaticCast<const AstNode *>(target) : this;
		}

		if (type == AST_IMPORT)
			return nodes[0];
	}

	for (Int i=0; i<nodes.GetSize(); i++)
	{
		const AstNode *n = nodes[i]->FindDefinition(col, line, filename);

		if (n)
			return n;
	}

	return 0;
}

AstNode *AstNode::Add(AstNode *n)
{
	LETHE_ASSERT(n && !n->parent);
	n->cachedIndex = nodes.Add(n);
	n->parent = this;
	return n;
}

AstNode *AstNode::UnbindNode(Int idx)
{
	AstNode *res = nodes[idx];
	nodes[idx] = nullptr;
	res->parent = nullptr;
	res->cachedIndex = -1;
	return res;
}

AstNode *AstNode::BindNode(Int idx, AstNode *n)
{
	LETHE_ASSERT(!n->parent && !nodes[idx]);
	n->parent = this;
	n->cachedIndex = idx;
	nodes[idx] = n;
	return n;
}

bool AstNode::ReplaceChild(AstNode *oldc, AstNode *newc)
{
	Int start = oldc ? oldc->cachedIndex : 0;

	start *= (UInt)start < (UInt)nodes.GetSize() && nodes[start] == oldc;

	for (Int i=start; i<nodes.GetSize(); i++)
	{
		if (nodes[i] == oldc)
		{
			if (!newc)
			{
				nodes.EraseIndex(i);

				// we need to invalidate cached indices for the remaning nodes
				for (Int j=i; j<nodes.GetSize(); j++)
					nodes[j]->cachedIndex = -1;

				return true;
			}

			newc->parent = this;

			if (oldc)
				newc->location = oldc->location;

			newc->cachedIndex = i;
			nodes[i] = newc;

			return true;
		}
	}

	return false;
}

bool AstNode::IsRightAssocBinaryOp() const
{
	switch(type)
	{
	case AST_OP_ASSIGN:
	case AST_OP_ADD_ASSIGN:
	case AST_OP_SUB_ASSIGN:
	case AST_OP_MUL_ASSIGN:
	case AST_OP_DIV_ASSIGN:
	case AST_OP_MOD_ASSIGN:
	case AST_OP_SHL_ASSIGN:
	case AST_OP_SHR_ASSIGN:
	case AST_OP_AND_ASSIGN:
	case AST_OP_XOR_ASSIGN:
	case AST_OP_OR_ASSIGN:
	case AST_OP_SWAP:
	case AST_OP_TERNARY:
		return 1;

	default:
		;
	}

	return 0;
}

AstNode *AstNode::ConvertConstTo(DataTypeEnum, const CompiledProgram &)
{
	return this;
}

bool AstNode::IsConstant() const
{
	return type >= AST_CONST_BOOL && type <= AST_CONST_STRING;
}

bool AstNode::HasSideEffects() const
{
	if (IsConstant())
		return false;

	if (!nodes.IsEmpty())
		return true;

	if (type != AST_IDENT || !target)
		return true;

	switch(target->type)
	{
	case AST_ARG:
		// arg load never has a side effect
		return false;
	case AST_VAR_DECL:
		// no side effects unless virtual prop
		if (!(target->qualifiers & AST_Q_PROPERTY))
			return false;
		// fallthrough
	default:
		return true;
	}
}

Int AstNode::ToBoolConstant(const CompiledProgram &)
{
	return -1;
}

bool AstNode::IsZeroConstant(const CompiledProgram &) const
{
	return false;
}

AstNode *AstNode::DerefConstant(const CompiledProgram &)
{
	return 0;
}

bool AstNode::IsConstExpr() const
{
	for (Int i=0; i<nodes.GetSize(); i++)
		LETHE_RET_FALSE(nodes[i]->IsConstExpr());

	return 1;
}

bool AstNode::BeginCodegen(CompiledProgram &p)
{
	for (auto n : nodes)
		LETHE_RET_FALSE(n->BeginCodegen(p));

	return true;
}

bool AstNode::FoldConst(const CompiledProgram &p)
{
	bool res = false;

	for (Int i=0; i<nodes.GetSize(); i++)
		res |= nodes[i]->FoldConst(p);

	return res;
}

bool AstNode::ResolveNode(const ErrorHandler &)
{
	return true;
}

AstNode::ResolveResult AstNode::Resolve(const ErrorHandler &e)
{
	return ResolveFrom(e, 0);
}

AstNode::ResolveResult AstNode::ResolveFrom(const ErrorHandler &e, Int fromIdx)
{
	ResolveResult res = RES_OK;

	if (!ResolveNode(e))
	{
		e.Error(this, "cannot resolve node");
		return RES_ERROR;
	}

	// FIXME: is this necessary AT ALL?!
	if ((type == AST_OP_DOT || type == AST_OP_SCOPE_RES) && !IsResolved())
	{
		// can't go deeper until dot/scope res is resolved
		if (!nodes[1]->IsResolved())
			return RES_OK;
	}

	bool resolved = !nodes.IsEmpty();

	for (Int idx=fromIdx; idx<nodes.GetSize(); idx++)
	{
		Int i = idx;

		ResolveResult nr = nodes[i]->Resolve(e);

		if (nr == RES_ERROR)
			return nr;

		if (nr == RES_MORE)
			res = nr;

		resolved &= (nodes[i]->flags & AST_F_RESOLVED) != 0;
	}

	if (resolved && !(flags & AST_F_RESOLVED))
	{
		flags |= AST_F_RESOLVED;
		res = RES_MORE;
	}

	return res;
}

bool AstNode::IsResolved() const
{
	if (qualifiers & AST_Q_TEMPLATE)
		return true;

	for (Int i=0; i<nodes.GetSize(); i++)
		LETHE_RET_FALSE(nodes[i]->IsResolved());

	return (flags & AST_F_RESOLVED) != 0;
}

String AstNode::GetTextRepresentation() const
{
	if (type == AST_CONST_INT)
	{
		// FIXME: hack!
		return String::Printf("[%s] %d", AST_TYPE_NAMES[type], num.i);
	}

	return String::Printf("[%s]", AST_TYPE_NAMES[type]);
}

const NamedScope *AstNode::GetSymScope(bool parentOnly) const
{
	const AstNode *n = this;

	if (qualifiers & AST_Q_CONTEXT_SYMBOL)
	{
		auto *rt = parent->GetContextTypeNode(this);

		if (rt && rt->type == AST_ENUM_ITEM)
			rt = rt->parent;

		if (rt && rt->type == AST_ENUM && rt->nodes.GetSize() > 1)
			return rt->nodes[1]->scopeRef;

		return nullptr;
	}

	if (parentOnly)
		return parent->symScopeRef;

	while (n)
	{
		if (n->symScopeRef)
			return n->symScopeRef;

		n = n->parent;
	}

	return scopeRef;
}

bool AstNode::IsElemType() const
{
	return type >= AST_TYPE_VOID && type <= AST_TYPE_STRING;
}

AstNodeType AstNode::PromoteSmallType(AstNodeType ntype)
{
	switch(ntype)
	{
	case AST_TYPE_BOOL:
	case AST_TYPE_SBYTE:
	case AST_TYPE_BYTE:
	case AST_TYPE_SHORT:
	case AST_TYPE_USHORT:
		ntype = AST_TYPE_INT;
		break;
	default:;
	}
	return ntype;
}

const AstNode *AstNode::GetTypeNode() const
{
	return IsElemType() ? this : nullptr;
}

const AstNode *AstNode::GetContextTypeNode(const AstNode *) const
{
	return nullptr;
}

AstNode *AstNode::FindSymbolNode(String &, const NamedScope *&) const
{
	return nullptr;
}

AstSymbol *AstNode::FindVarSymbolNode(bool)
{
	return nullptr;
}

QDataType AstNode::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_NONE];
	return res;
}

void AstNode::Dump(Stream &s, Int level) const
{
	String tmp;

	for (Int j=0; j<level; j++)
		tmp += "    ";

	tmp += GetTextRepresentation();
	String tmp2;
	tmp2.Format(" (resolved:%d)", (flags & AST_F_RESOLVED) != 0);
	tmp += tmp2;
	tmp += "\n";
	s.Write(tmp.Ansi(), tmp.GetLength());

	for (Int i=0; i<nodes.GetSize(); i++)
		nodes[i]->Dump(s, level+1);
}

// AstNode

bool AstNode::ValidateMethod(CompiledProgram &p, const NamedScope *nscope, AstNode *nfunc) const
{
	auto thisScope = nfunc->scopeRef ? nfunc->scopeRef->FindThis() : nullptr;

	if (!thisScope || !thisScope->IsBaseOf(nscope))
	{
		// we still want to allow classes that are compatible with any base of this (i.e. state classes),
		// because state classes cannot add new members/virtual methods
		if (thisScope && (thisScope->node->qualifiers & AST_Q_STATE) &&
			thisScope->base && thisScope->base->IsBaseOf(nscope))
		{
				return true;
		}

		return p.Error(this, "foreign method not accessible from here");
	}

	return true;
}

bool AstNode::CodeGen(CompiledProgram &p)
{
	if (flags & AST_F_SKIP_CGEN)
		return true;

	for (Int i=0; i<nodes.GetSize(); i++)
	{
		p.SetLocation(nodes[i]->location);
		LETHE_RET_FALSE(nodes[i]->CodeGen(p));
	}

	return true;
}

bool AstNode::CodeGenNoBreak(CompiledProgram &p)
{
	// make sure break/continue in state break go where they need
	// temporarily fool BreakScope lookup
	p.curScope->type = NSCOPE_LOCAL;
	auto res = CodeGen(p);
	p.curScope->type = NSCOPE_LOOP;
	return res;
}

bool AstNode::GenInitializerList(CompiledProgram &, QDataType, Int, bool)
{
	return false;
}

bool AstNode::IsCompleteInitializerList(CompiledProgram &, QDataType) const
{
	return false;
}

bool AstNode::IsInitializerConst(const CompiledProgram &, QDataType) const
{
	return false;
}

bool AstNode::CodeGenGlobalCtorStatic(CompiledProgram &p)
{
	if (qualifiers & AST_Q_TEMPLATE)
		return true;

	if (type == AST_VAR_DECL_LIST && (nodes[0]->qualifiers & AST_Q_STATIC))
	{
		LETHE_RET_FALSE(CodeGen(p));

		flags |= AST_F_SKIP_CGEN;
		return true;
	}

	for (Int i=0; i<nodes.GetSize(); i++)
	{
		AstNode *n = nodes[i];
		LETHE_RET_FALSE(n->CodeGenGlobalCtorStatic(p));
	}

	return true;
}

bool AstNode::CodeGenGlobalCtor(CompiledProgram &p)
{
	if (type == AST_PROGRAM_LIST)
	{
		p.globalConstIndex = p.instructions.GetSize();
		p.EmitFunc("..global_ctor", 0);
	}

	for (Int i=0; i<nodes.GetSize(); i++)
	{
		AstNode *n = nodes[i];

		if (n->scopeRef && !n->scopeRef->IsGlobal())
		{
			LETHE_RET_FALSE(n->CodeGenGlobalCtorStatic(p));
			continue;
		}

		if (n->type == AST_VAR_DECL_LIST)
		{
			LETHE_RET_FALSE(n->CodeGen(p));
			n->flags |= AST_F_SKIP_CGEN;
		}
		else
			LETHE_RET_FALSE(n->CodeGenGlobalCtor(p));
	}

	if (type == AST_PROGRAM_LIST)
	{
		p.Emit(OPC_RET);
		p.FlushOpt();
	}

	return true;
}

bool AstNode::CodeGenComposite(CompiledProgram &p)
{
	for (Int i=0; i<nodes.GetSize(); i++)
		LETHE_RET_FALSE(nodes[i]->CodeGenComposite(p));

	return true;
}

bool AstNode::CodeGenRef(CompiledProgram &p, bool, bool)
{
	return p.Error(this, "not an lvalue");
}

bool AstNode::TypeGen(CompiledProgram &p)
{
	for (Int i=0; i<nodes.GetSize(); i++)
		LETHE_RET_FALSE(nodes[i]->TypeGen(p));

	return true;
}

bool AstNode::TypeGenDef(CompiledProgram &p)
{
	for (Int i=0; i<nodes.GetSize(); i++)
		LETHE_RET_FALSE(nodes[i]->TypeGenDef(p));

	return true;
}

bool AstNode::VtblGen(CompiledProgram &p)
{
	for (Int i=0; i<nodes.GetSize(); i++)
		LETHE_RET_FALSE(nodes[i]->VtblGen(p));

	return true;
}

bool AstNode::EmitPtrLoad(const QDataType &dt, CompiledProgram &p)
{
	QDataType tmp = dt;
	tmp.RemoveReference();

	// stack: [0] = adr

	const auto dte = dt.GetTypeEnum();

	if (dt.IsPointer())
	{
		if (dte == DT_WEAK_PTR)
		{
			// special handling: zero src ptr if expired
			p.EmitI24(OPC_BCALL, BUILTIN_FIX_WEAK_REF );
		}

		p.Emit(OPC_PLOADPTR_IMM);
		tmp.qualifiers |= AST_Q_SKIP_DTOR;
	}
	else if (dte == DT_STRUCT)
	{
		bool hasDtor = tmp.HasDtor();
		Int stkSize = (tmp.GetSize() + Stack::WORD_SIZE-1)/Stack::WORD_SIZE;

		// FIXME: stupid stack!!! this is very hacky...
		if (stkSize > 1)
			p.EmitU24Zero(hasDtor ? OPC_PUSHZ_RAW : OPC_PUSH_RAW, stkSize-1);

		// push source adr
		p.EmitU24(OPC_LPUSHPTR, stkSize-1);

		if (hasDtor)
		{
			// zero again...
			if constexpr (Stack::WORD_SIZE > 4)
			{
				p.EmitI24(OPC_PUSHZ_RAW, 1);
				p.EmitU24(OPC_LSTOREPTR, stkSize+1);
			}
			else
			{
				p.Emit(OPC_PUSH_ICONST);
				p.EmitU24(OPC_LSTORE32, stkSize+1);
			}
		}

		// push dest adr
		p.EmitI24(OPC_LPUSHADR, 1);

		if (hasDtor)
		{
			p.EmitBackwardJump(OPC_CALL, dt.GetType().funAssign);
			p.EmitI24(OPC_POP, 2);
		}
		else
			p.EmitU24(OPC_PCOPY, tmp.GetSize());
	}
	else if (dte <= DT_STRING)
	{
		p.Emit(OPC_PUSH_ICONST);

		// FIXME: more types...
		if (dte == DT_STRING)
			p.EmitI24(OPC_BCALL, BUILTIN_PLOADSTR);
		else
		{
			if (dt.IsMethodPtr())
				return p.Error(this, "cannot load method");

			p.EmitI24(opcodeRefLoadOfs[dt.GetTypeEnumUnderlying()], 1);
		}
	}
	else if (dte == DT_ARRAY_REF || dte == DT_DELEGATE || dte == DT_DYNAMIC_ARRAY)
	{
		p.Emit(OPC_LPUSHPTR);
		p.Emit(OPC_PLOADPTR_IMM);
		p.EmitI24(OPC_LPUSHPTR, 1);

		if (dte == DT_DELEGATE)
		{
			p.EmitI24(OPC_PLOADPTR_IMM, (Int)sizeof(void *));
			p.EmitI24(OPC_LSTOREPTR, 2);
		}
		else
		{
			p.EmitI24(OPC_PLOAD32_IMM, (Int)sizeof(void *));
			p.EmitI24(OPC_LSTORE32, 2);
		}

		if (dte == DT_DYNAMIC_ARRAY)
		{
			tmp.ref = tmp.ref->complementaryType;
			tmp.qualifiers &= ~AST_Q_DTOR;
		}
	}
	else
	{
		// force reference
		tmp.qualifiers |= AST_Q_REFERENCE;
	}

	p.PushStackType(tmp);
	return true;
}

Array<AstNode::AdlResolveData> AstNode::GetAdlResolveNodes()
{
	Array<AstNode::AdlResolveData> resData;
	GetAdlResolveNodesInternal(resData, 0);
	resData.Sort();
	return resData;
}

void AstNode::GetAdlResolveNodesInternal(Array<AdlResolveData> &resData, Int ndepth)
{
	// ignore non-instantiated templates
	if (qualifiers & AST_Q_TEMPLATE)
		return;

	if (type == AST_VAR_DECL || type == AST_CALL || !(flags & AST_F_RESOLVED))
		if (!IsResolved())
			resData.Add(AdlResolveData{this, ndepth});

	for (auto &&it : nodes)
		it->GetAdlResolveNodesInternal(resData, ndepth+1);
}

void AstNode::AppendTypeQualifiers(StringBuilder &sb) const
{
	if (qualifiers & AST_Q_CONST)
		sb.AppendFormat("const ");

	if (qualifiers & AST_Q_RAW)
		sb.AppendFormat("raw ");

	if (qualifiers & AST_Q_WEAK)
		sb.AppendFormat("weak ");
}

bool AstNode::GetTemplateTypeText(StringBuilder &) const
{
	return false;
}

const AstNode *AstNode::FindTemplate() const
{
	auto *insideTemplate = this;

	while (insideTemplate && !(insideTemplate->qualifiers & (AST_Q_TEMPLATE | AST_Q_TEMPLATE_INSTANTIATED)))
		insideTemplate = insideTemplate->parent;

	if (insideTemplate && (insideTemplate->type == AST_STRUCT || insideTemplate->type == AST_CLASS))
		return insideTemplate;

	return nullptr;
}

String AstNode::FindTemplateName() const
{
	auto *insideTemplate = FindTemplate();

	if (insideTemplate)
	{
		auto oname = AstStaticCast<const AstTypeStruct *>(insideTemplate)->overrideName;

		if (!oname.IsEmpty())
			return oname;

		return AstStaticCast<AstText *>(insideTemplate->nodes[0])->GetQTextSlow();
	}

	return String();
}

AstNode *AstNode::ResolveTemplateScope(AstNode *&) const
{
	return nullptr;
}

void AstNode::FixPointerQualifiers(const CompiledProgram &p, QDataType &ntype, const AstNode *nnode)
{
	if (!ntype.IsPointer())
		return;

	if (nnode->type == AST_NEW)
	{
		ntype.qualifiers &= ~AST_Q_SKIP_DTOR;
		return;
	}

	if (nnode->type == AST_CALL)
	{
		auto *fn = AstStaticCast<const AstCall *>(nnode)->GetFuncBase();

		if (!fn)
			return;

		auto *rn = fn->GetResult();

		auto qt = rn->GetTypeDesc(p);

		if (!qt.IsReference())
			ntype.qualifiers &= ~AST_Q_SKIP_DTOR;
	}
}

DataTypeEnum AstNode::TypeEnumFromNode(const AstNode *n)
{
	switch(n->type)
	{
	case AST_TYPE_BOOL:
		return DT_BOOL;

	case AST_TYPE_BYTE:
		return DT_BYTE;

	case AST_TYPE_SBYTE:
		return DT_SBYTE;

	case AST_TYPE_SHORT:
		return DT_SHORT;

	case AST_TYPE_USHORT:
		return DT_USHORT;

	case AST_TYPE_CHAR:
		return DT_CHAR;

	case AST_ENUM_ITEM:
		return TypeEnumFromNode(n->parent->nodes[AstTypeEnum::IDX_UNDERLYING]);

	case AST_TYPE_INT:
		return DT_INT;

	case AST_TYPE_UINT:
		return DT_UINT;

	case AST_TYPE_LONG:
		return DT_LONG;

	case AST_TYPE_ULONG:
		return DT_ULONG;

	case AST_TYPE_FLOAT:
		return DT_FLOAT;

	case AST_TYPE_DOUBLE:
		return DT_DOUBLE;

	case AST_TYPE_NAME:
		return DT_NAME;

	case AST_TYPE_STRING:
		return DT_STRING;

	case AST_TYPE_ARRAY_REF:
		return DT_ARRAY_REF;

	default:;
	}

	return DT_NONE;
}

const AstNode *AstNode::CoerceTypes(const AstNode *type0, const AstNode *type1)
{
	LETHE_RET_FALSE(type0 && type1);

	auto dte0 = TypeEnumFromNode(type0);
	auto dte1 = TypeEnumFromNode(type1);

	if (dte0 == DT_NONE || dte1 == DT_NONE)
		return nullptr;

	auto cdte = DataType::ComposeTypeEnum(dte0, dte1);

	if (cdte == DT_NONE)
		return nullptr;

	if (cdte == DT_INT && cdte != dte0 && cdte != dte1)
	{
		static AstTypeInt tnode{TokenLocation()};
		return &tnode;
	}

	return cdte == dte0 ? type0 : type1;
}

AstNode *AstNode::Clone() const
{
	auto *res = new AstNode;
	AstNode::CopyTo(res);
	return res;
}

void AstNode::CopyTo(AstNode *n) const
{
	n->parent = nullptr;
	n->target = target;
	n->type = type;
	n->flags = flags;
	n->offset = offset;
	n->scopeRef = scopeRef;
	n->symScopeRef = symScopeRef;
	n->qualifiers = qualifiers;
	n->num = num;
	n->location = location;
	n->cachedIndex = -1;
	// clone nodes

	n->nodes.Clear();
	n->nodes.Resize(nodes.GetSize(), nullptr);

	for (Int i=0; i<nodes.GetSize(); i++)
	{
		auto *cn = nodes[i]->Clone();
		n->nodes[i] = cn;
		cn->parent = n;
	}
}

}
