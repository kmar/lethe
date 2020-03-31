#include "AstSubscriptOp.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Ast/NamedScope.h>
#include <Lethe/Script/Vm/Stack.h>
#include <Lethe/Script/Ast/CodeGenTables.h>
#include <Lethe/Script/Ast/Function/AstCall.h>
#include <Lethe/Script/Ast/Function/AstFunc.h>
#include "../AstText.h"
#include "AstBinaryOp.h"

namespace lethe
{

// AstSubscriptOp

AstNode *AstSubscriptOp::FindSymbolNode(String &sname, const NamedScope *&nscope) const
{
	AstNode *res = nodes[0]->FindSymbolNode(sname, nscope);

	if (res)
	{
		LETHE_ASSERT(res->type == AST_VAR_DECL);
		res = res->parent->nodes[0]->nodes[0];
	}

	return res;
}

AstNode *AstSubscriptOp::FindVarSymbolNode()
{
	return nodes[0]->FindVarSymbolNode();
}

QDataType AstSubscriptOp::GetTypeDesc(const CompiledProgram &p) const
{
	// [0] = what to deref
	// [1] = index expr
	auto leftType = nodes[0]->GetTypeDesc(p);

	auto qdt = leftType.GetType().elemType;

	if (leftType.IsStruct())
	{
		// look up operator
		auto *scope = leftType.GetType().structScopeRef;
		auto *op = scope ? scope->FindOperator(p, "[]", leftType, nodes[1]->GetTypeDesc(p)) : nullptr;

		if (op)
			qdt = AstStaticCast<AstFunc *>(op)->GetResult()->GetTypeDesc(p);
		else if (leftType.IsIndexableStruct())
			qdt = leftType.GetType().members[0].type;
	}

	if (!qdt.IsReference() && qdt.GetTypeEnum() == DT_STRONG_PTR)
		qdt.qualifiers |= AST_Q_SKIP_DTOR;

	return qdt;
}

const AstNode *AstSubscriptOp::GetTypeNode() const
{
	const auto *res = GetResolveTarget();

	if (res)
		res = res->GetTypeNode();

	return res;
}

AstNode *AstSubscriptOp::GetResolveTarget() const
{
	// extra handling needed
	auto ntarg = nodes[0]->GetResolveTarget();
	LETHE_RET_FALSE(ntarg);

	const auto *tn = ntarg->GetTypeNode();
	LETHE_RET_FALSE(tn);

	if (tn->type == AST_TYPE_ARRAY || tn->type == AST_TYPE_DYNAMIC_ARRAY || tn->type == AST_TYPE_ARRAY_REF)
		return tn->nodes[0]->GetResolveTarget();

	if (tn->type == AST_STRUCT && tn->scopeRef)
	{
		auto *tmp = AstBinaryOp::FindUserDefOperatorType("[]", tn, nodes[1]->GetTypeNode());

		if (tmp)
			tn = tmp;
		else
		{
			// assume indexable struct or using an operator
			const auto &m = tn->scopeRef->members;

			if (!m.IsEmpty())
				return m.GetValue(0)->GetResolveTarget();
		}
	}

	return const_cast<AstNode *>(tn);
}

bool AstSubscriptOp::CodeGenSubscript(CompiledProgram &p, bool store, bool allowConst, bool derefPtr)
{
	// FIXME: better!
	auto qdt = nodes[0]->GetTypeDesc(p);

	const bool nobounds = (qdt.qualifiers & AST_Q_NOBOUNDS) || p.GetUnsafe();

	if (qdt.GetTypeEnum() == DT_STRING)
	{
		// special handling for strings...
		if (!allowConst || store)
			return p.Error(this, "cannot modify string characters");

		LETHE_RET_FALSE(nodes[0]->CodeGenRef(p, allowConst));
		LETHE_ASSERT(p.exprStack.GetSize() >= 1);

		// need to eval full expression
		LETHE_RET_FALSE(nodes[1]->CodeGen(p));
		// convert to int
		LETHE_RET_FALSE(p.EmitConv(nodes[1],
								  nodes[1]->GetTypeDesc(p), p.elemTypes[DT_INT]));

		p.Emit(OPC_BCALL + (BUILTIN_GETSTRCHAR << 8));

		if (!nobounds)
			p.EmitU24(OPC_RANGE_ICONST, 256);

		p.PopStackType();
		p.PopStackType();
		p.PushStackType(GetTypeDesc(p));
		return true;
	}

	DataType structAsStatic;

	if (qdt.IsIndexableStruct())
	{
		auto *scope = qdt.GetType().structScopeRef;

		bool hasIndexOp = false;

		if (scope)
		{
			for (auto *it : scope->operators)
			{
				auto *fn = AstStaticCast<AstFunc *>(it);

				if (AstStaticCast<AstText *>(fn->nodes[AstFunc::IDX_NAME])->text == "[]")
				{
					hasIndexOp = true;
					break;
				}
			}
		}

		if (!hasIndexOp)
		{
			Int count = qdt.GetType().members.GetSize();
			auto elemType = qdt.GetType().members[0].type;

			structAsStatic.type = DT_STATIC_ARRAY;
			structAsStatic.align = elemType.GetAlign();
			structAsStatic.size = count * elemType.GetSize();
			structAsStatic.elemType = elemType;
			structAsStatic.arrayDims = count;

			auto q = qdt.qualifiers;
			qdt = QDataType::MakeConstType(structAsStatic);
			qdt.qualifiers = q;
		}
	}

	if (!qdt.IsArray())
	{
		if (qdt.IsStruct())
		{
			auto *scope = qdt.GetType().structScopeRef;
			LETHE_ASSERT(scope);
			auto *op = scope->FindOperator(p, "[]", qdt, nodes[1]->GetTypeDesc(p));

			if (!op)
				return p.Error(this, "matching operator not found!");

			auto res = AstCall::CallOperator(2, p, this, op, store);

			if (p.exprStack.GetSize() <= 0)
				return p.Error(this, "subscript op must return a value");

			if (store && !p.exprStack.Back().IsReference())
				return p.Error(this, "subscript op must return a reference for store");

			return res;
		}

		return p.Error(this, "cannot subscript non-array types");
	}

	if (!allowConst && qdt.IsConst())
		return p.Error(this, "cannot modify constant");

	const DataType &dt = qdt.GetType();

	LETHE_RET_FALSE(nodes[0]->CodeGenRef(p, allowConst));

	if (dt.type != DT_STATIC_ARRAY)
	{
		if (!nobounds)
			p.Emit(OPC_LPUSHPTR);

		auto &last = p.instructions.Back();

		if ((last & 255) == OPC_LPUSHADR)
		{
			last &= ~255;
			last |= OPC_LPUSHPTR;
		}
		else
			p.EmitU24(OPC_PLOADPTR_IMM, 0);

		if (!nobounds)
		{
			p.Emit(OPC_LSWAPPTR);
			p.EmitU24(OPC_PLOAD32_IMM, Stack::WORD_SIZE);
			p.PushStackType(QDataType::MakeType(p.elemTypes[DT_FUNC_PTR]));
		}
	}

	QDataType elemType = GetTypeDesc(p);

	bool premult = 0;

	if (nodes[1]->IsConstant())
	{
		// direct access, special handling
		if (!nodes[1]->ConvertConstTo(DT_INT, p))
			return p.Error(nodes[1], "cannot convert constant");

		Int idx = nodes[1]->num.i;

		// check max range as well
		if (dt.type == DT_STATIC_ARRAY && (idx < 0 || idx >= dt.arrayDims))
			return p.Error(this, "constant array index out of range");

		if (dt.type != DT_STATIC_ARRAY && !nobounds)
		{
			// perform range check now...
			p.PopStackType();
			p.EmitIntConst(idx);
			p.Emit(OPC_RANGE);
			p.PushStackType(QDataType::MakeType(p.elemTypes[DT_INT]));
		}
		else
		{
			Int absOfs = elemType.GetSize() * idx;
			// FIXME: better!
			p.EmitIntConst(absOfs);
			p.PushStackType(QDataType::MakeType(p.elemTypes[DT_INT]));
			premult = 1;
		}
	}
	else
	{
		// need to eval full expression
		LETHE_RET_FALSE(nodes[1]->CodeGen(p));
		// convert to int
		LETHE_RET_FALSE(p.EmitConv(nodes[1],
								  nodes[1]->GetTypeDesc(p), p.elemTypes[DT_INT]));

		if (dt.type != DT_STATIC_ARRAY && !nobounds)
		{
			auto top = p.exprStack.Back();
			p.PopStackType();
			p.PopStackType();
			p.PushStackType(top);
			// perform range check now...
			p.Emit(OPC_RANGE);
		}

		// now if last instruction is push_iconst, try to avoid range check
		Int lastIns = p.instructions.Back();
		UInt lastOpc = lastIns & 255u;

		if (lastOpc == OPC_PUSH_ICONST)
		{
			Int idx = lastIns >> 8;

			// check max range as well
			if (idx < 0 || idx >= dt.arrayDims)
				return p.Error(this, "constant array index out of range");
		}
		else if (!nobounds && dt.type == DT_STATIC_ARRAY)
		{
			// perform range check
			if (dt.arrayDims >= (1 << 24))
				p.EmitU24(OPC_RANGE_CICONST, p.cpool.Add(dt.arrayDims));
			else
				p.EmitU24(OPC_RANGE_ICONST, dt.arrayDims);
		}
	}

	LETHE_ASSERT(p.exprStack.GetSize() >= 2);

	// now emit indirect pointer fetch
	LETHE_ASSERT(!elemType.IsReference() && /*elemType.GetTypeEnum() <= DT_STRING &&*/ elemType.GetSize() > 0);

	Int sz = premult ? 1 : elemType.GetSize();

	if (sz > 0xffffff)
		return p.Error(this, "array element size too big");

	if (store || elemType.GetTypeEnum() == DT_STATIC_ARRAY)
		p.EmitU24(OPC_AADD, sz);
	else
	{
		// TODO: more ...
		switch(elemType.GetTypeEnum())
		{
		case DT_STRING:

			// handle strings now...
			if (!premult)
				p.EmitI24(OPC_IMUL_ICONST, sz);

			p.Emit(OPC_BCALL + (BUILTIN_PLOADSTR << 8));
			break;

		case DT_STRUCT:
		case DT_RAW_PTR:
		case DT_STRONG_PTR:
		case DT_WEAK_PTR:
		case DT_DELEGATE:
		case DT_ARRAY_REF:
		case DT_DYNAMIC_ARRAY:
			p.EmitI24(OPC_AADD, sz);
			p.PopStackType(1);
			p.PopStackType(1);
			return EmitPtrLoad(elemType, p);

		default:
			if (elemType.GetTypeEnum() <= DT_FUNC_PTR)
			{
				if (elemType.IsMethodPtr())
					return p.Error(this, "cannot load method");

				p.EmitU24(opcodeRefLoadOfs[elemType.GetTypeEnum()], sz);
			}
			else
				return p.Error(this, "unsupported subscript elem type");
		}
	}

	if (derefPtr && elemType.IsPointer())
		p.Emit(OPC_PLOADPTR_IMM);

	p.PopStackType(1);
	p.PopStackType(1);

	if (elemType.IsArray() || store)
	{
		// force reference
		elemType.qualifiers |= AST_Q_REFERENCE;
	}

	p.PushStackType(elemType);
	return true;
}

bool AstSubscriptOp::CodeGen(CompiledProgram &p)
{
	return CodeGenSubscript(p, 0, 1);
}

bool AstSubscriptOp::CodeGenRef(CompiledProgram &p, bool allowConst, bool derefPtr)
{
	return CodeGenSubscript(p, 1, allowConst, derefPtr);
}


}
