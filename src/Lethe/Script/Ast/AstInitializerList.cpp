#include "AstInitializerList.h"
#include "BinaryOp/AstAssignOp.h"
#include "NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/Vm/Stack.h>

namespace lethe
{

// AstInitializerList

bool AstInitializerList::IsCompleteInitializedElem(CompiledProgram &p, AstNode *n, QDataType elem) const
{
	if (elem.HasCtor() || elem.HasDtor())
		return false;

	if (n->type == AST_INITIALIZER_LIST)
	{
		// recurse...
		return n->IsCompleteInitializerList(p, elem);
	}

	return true;
}

bool AstInitializerList::IsElemConst(const CompiledProgram &p, AstNode *n, QDataType elem) const
{
	LETHE_RET_FALSE(elem.GetTypeEnum() == DT_STRING || (!elem.HasCtor() && !elem.HasDtor()));

	if (n->type == AST_INITIALIZER_LIST)
		return n->IsInitializerConst(p, elem);

	n = n->ConvertConstTo(elem.GetTypeEnum(), p);
	LETHE_RET_FALSE(n);

	return n->IsConstant();
}

bool AstInitializerList::GenInitializeElem(CompiledProgram &p, AstNode *n, QDataType elem, Int ofs, bool global)
{
	if (n->type == AST_INITIALIZER_LIST)
		return n->GenInitializerList(p, elem, ofs, global);

	// eval node!
	QDataType ntype = n->GetTypeDesc(p);

	n = n->ConvertConstTo(elem.GetTypeEnum(), p);
	LETHE_RET_FALSE(n);

	if (global && n->IsConstant())
		return BakeGlobalData(n, elem, ofs, p);

	LETHE_RET_FALSE(n->CodeGen(p));

	if (p.exprStack.GetSize() < 1)
		return p.Error(this, "initializer expression must return a value");

	auto top = p.exprStack.Back();

	// perform conversion if necessary
	if (elem.GetType() != top.GetType())
	{
		auto oldWords = Stack::AlignSize(top.GetSize());
		LETHE_RET_FALSE(p.EmitConv(n, top, elem.GetType()));
		top = p.exprStack.Back();
		auto newWords = Stack::AlignSize(top.GetSize());

		if (!global)
			ofs += newWords - oldWords;
	}

	if (!elem.CanAlias(ntype))
		return p.Error(n, "invalid type");

	p.DynArrayVarFix(ntype);

	// FIXME: refactor (merge) this copy-pasted code with code in AstVarDecl
	if (!global && p.GetJitFriendly() && top.HasArrayRefWithNonConstElem())
	{
		// we want to solve a problem for JIT; find leftmost symbol AND mark it as REF_ALIASED
		// later when generating code this should force a reload via pointer using lpushadr/ptrload
		// we don't want to do this for the interpreter however!
		// another thing which remains is array ref filled from local array
		// even if we can't catch every corner case, this should get rid of "surprises"
		auto *snode = n->FindVarSymbolNode();

		if (snode && snode->target && snode->scopeRef && snode->scopeRef->IsLocal())
			snode->target->qualifiers |= AST_Q_REF_ALIASED;
	}

	bool structOnStack;
	Int ssize;
	QDataType rhs;
	LETHE_RET_FALSE(AstAssignOp::CodeGenPrepareAssign(p, rhs, structOnStack, ssize));

	// push adr
	if (global)
	{
		UInt gofs = ofs;
		p.EmitU24(OPC_GLOADADR, gofs);
	}
	else
	{
		Int lofs = p.exprStackOfs + ofs + p.initializerDelta;
		LETHE_ASSERT(lofs >= 0);
		UInt lofsWords = lofs / Stack::WORD_SIZE;

		p.EmitU24(OPC_LPUSHADR, lofsWords);
		Int remainder = lofs - lofsWords*Stack::WORD_SIZE;

		if (remainder)
			p.EmitI24(OPC_AADD_ICONST, remainder);
	}

	QDataType tmpref = elem;
	tmpref.qualifiers |= AST_Q_REFERENCE;
	p.PushStackType(tmpref);

	Int mark = p.ExprStackMark();
	LETHE_RET_FALSE(AstAssignOp::CodeGenDoAssign(n, p, elem, rhs, true, structOnStack, ssize));
	p.ExprStackCleanupTo(mark);

	return true;
}

bool AstInitializerList::IsInitializerConst(const CompiledProgram &p, QDataType qdt) const
{
	DataTypeEnum dte = qdt.GetTypeEnum();

	// FIXME: shares most of the code with GenInitializerList

	if (dte != DT_STATIC_ARRAY && dte != DT_STRUCT)
		return false;

	if (dte == DT_STATIC_ARRAY)
	{
		auto elem = qdt.GetType().elemType;

		for (Int i = 0; i < nodes.GetSize(); i++)
		{
			if (i >= qdt.GetType().arrayDims)
				return false;

			AstNode *n = nodes[i];

			LETHE_RET_FALSE(IsElemConst(p, n, elem));
		}

		return true;
	}

	LETHE_ASSERT(dte == DT_STRUCT);

	StackArray< const DataType *, 64 > bases;
	const DataType *cur = &qdt.GetType();
	Int numMembers = 0;

	while (cur)
	{
		int nmem = cur->members.GetSize();

		if (nmem)
		{
			bases.Add(cur);
			numMembers += nmem;
		}

		// intrinsic on struct means it's elems will be skipped in initializer list (because of map pair emulation)
		if (cur->baseType.GetTypeEnum() != DT_STRUCT || (cur->baseType.qualifiers & AST_Q_INTRINSIC))
			break;

		cur = &cur->baseType.GetType();
	}

	Int ibase = bases.GetSize() - 1;
	Int imember = 0;

	for (Int i=0; i<nodes.GetSize(); i++)
	{
		if (i >= numMembers)
			return false;

		const DataType &cbase = *bases[ibase];
		const DataType::Member &m = cbase.members[imember++];

		AstNode *n = nodes[i];

		LETHE_RET_FALSE(IsElemConst(p, n, m.type));

		if (imember >= cbase.members.GetSize())
		{
			imember = 0;
			ibase--;
		}
	}

	return true;
}

bool AstInitializerList::GenInitializerList(CompiledProgram &p, QDataType qdt, Int ofs, bool global)
{
	DataTypeEnum dte = qdt.GetTypeEnum();

	if (dte != DT_STATIC_ARRAY && dte != DT_STRUCT)
		return p.Error(this, "initializer list invalid for this type");

	if (dte == DT_STATIC_ARRAY)
	{
		const QDataType &elem = qdt.GetType().elemType;

		for (Int i=0; i<nodes.GetSize(); i++)
		{
			if (i >= qdt.GetType().arrayDims)
				return p.Error(this, "Too many initializers");

			AstNode *n = nodes[i];

			LETHE_RET_FALSE(GenInitializeElem(p, n, elem, ofs, global));

			ofs += elem.GetSize();
		}

		return 1;
	}

	LETHE_ASSERT(dte == DT_STRUCT);

	StackArray< const DataType *, 64 > bases;
	const DataType *cur = &qdt.GetType();
	Int numMembers = 0;

	while (cur)
	{
		int nmem = cur->members.GetSize();

		if (nmem)
		{
			bases.Add(cur);
			numMembers += nmem;
		}

		// intrinsic on struct means it's elems will be skipped in initializer list (because of map pair emulation)
		if (cur->baseType.GetTypeEnum() != DT_STRUCT || (cur->baseType.qualifiers & AST_Q_INTRINSIC))
			break;

		cur = &cur->baseType.GetType();
	}

	Int ibase = bases.GetSize()-1;
	Int imember = 0;

	Int baseOfs = ofs;

	for (Int i=0; i<nodes.GetSize(); i++)
	{
		if (i >= numMembers)
			return p.Error(this, "Too many initializers");

		const DataType &cbase = *bases[ibase];
		const DataType::Member &m = cbase.members[imember++];

		ofs = baseOfs + m.offset;

		AstNode *n = nodes[i];

		LETHE_RET_FALSE(GenInitializeElem(p, n, m.type, ofs, global));

		if (imember >= cbase.members.GetSize())
		{
			imember = 0;
			ibase--;
		}
	}

	return true;
}

bool AstInitializerList::IsCompleteInitializerList(CompiledProgram &p, QDataType qdt) const
{
	DataTypeEnum dte = qdt.GetTypeEnum();
	LETHE_RET_FALSE(dte == DT_STATIC_ARRAY || dte == DT_STRUCT);

	if (dte == DT_STATIC_ARRAY)
	{
		const QDataType &elem = qdt.GetType().elemType;

		LETHE_RET_FALSE(nodes.GetSize() == qdt.GetType().arrayDims);

		for (Int i = 0; i < nodes.GetSize(); i++)
		{
			AstNode *n = nodes[i];

			LETHE_RET_FALSE(IsCompleteInitializedElem(p, n, elem));
		}

		return true;
	}

	LETHE_ASSERT(dte == DT_STRUCT);

	Array< const DataType * > bases;
	const DataType *cur = &qdt.GetType();
	Int numMembers = 0;

	while (cur)
	{
		int nmem = cur->members.GetSize();

		if (nmem)
		{
			bases.Add(cur);
			numMembers += nmem;
		}

		if (cur->baseType.GetTypeEnum() != DT_STRUCT)
			break;

		// intrinsic on struct means it's elems will be skipped in initializer list (because of map pair emulation)
		// in that case, always zero-init!
		if (cur->baseType.qualifiers & AST_Q_INTRINSIC)
			return false;

		cur = &cur->baseType.GetType();
	}

	Int ibase = bases.GetSize() - 1;
	Int imember = 0;

	LETHE_RET_FALSE(nodes.GetSize() == numMembers);

	for (Int i = 0; i < nodes.GetSize(); i++)
	{
		const DataType &cbase = *bases[ibase];
		const DataType::Member &m = cbase.members[imember++];

		AstNode *n = nodes[i];

		LETHE_RET_FALSE(IsCompleteInitializedElem(p, n, m.type));

		if (imember >= cbase.members.GetSize())
		{
			imember = 0;
			ibase--;
		}
	}

	return true;
}


}
