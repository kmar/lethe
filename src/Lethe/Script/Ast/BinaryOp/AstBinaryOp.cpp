#include "AstBinaryOp.h"
#include "../NamedScope.h"
#include <Lethe/Script/Ast/Function/AstFunc.h>
#include <Lethe/Script/Ast/Function/AstCall.h>
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/Ast/Constants/AstConstBool.h>
#include <Lethe/Script/Ast/Constants/AstConstInt.h>
#include <Lethe/Script/Ast/Constants/AstConstUInt.h>
#include <Lethe/Script/Ast/Constants/AstConstLong.h>
#include <Lethe/Script/Ast/Constants/AstConstULong.h>
#include <Lethe/Script/Ast/Constants/AstConstFloat.h>
#include <Lethe/Script/Ast/Constants/AstConstDouble.h>
#include <Lethe/Script/Ast/Constants/AstConstName.h>
#include <Lethe/Script/Ast/Constants/AstConstString.h>
#include <Lethe/Script/Ast/AstText.h>

namespace lethe
{

AstTypeBool AstBinaryOp::boolType(TokenLocation{0, 0, String()});

static AstNode *AstCreateConstNode(DataTypeEnum dt, const TokenLocation &loc)
{
	switch(dt)
	{
	case DT_BOOL:
		return new AstConstBool(loc);

	case DT_BYTE:
	case DT_SBYTE:
	case DT_SHORT:
	case DT_USHORT:
	case DT_INT:
		return new AstConstInt(loc);

	case DT_UINT:
		return new AstConstUInt(loc);

	case DT_LONG:
		return new AstConstLong(loc);

	case DT_ULONG:
		return new AstConstULong(loc);

	case DT_FLOAT:
		return new AstConstFloat(loc);

	case DT_DOUBLE:
		return new AstConstDouble(loc);

	case DT_NAME:
		return new AstConstName("", loc);

	case DT_STRING:
		return new AstConstString("", loc);

	default:
		;
	}

	return nullptr;
}

// AstBinaryOp

bool AstBinaryOp::IsCompare() const
{
	switch(type)
	{
	case AST_OP_EQ:
	case AST_OP_NEQ:
	case AST_OP_LT:
	case AST_OP_LEQ:
	case AST_OP_GT:
	case AST_OP_GEQ:
		return true;

	default:
		;
	}

	return false;
}

bool AstBinaryOp::IsShift() const
{
	switch(type)
	{
	case AST_OP_SHL:
	case AST_OP_SHL_ASSIGN:
	case AST_OP_SHR:
	case AST_OP_SHR_ASSIGN:
		return true;

	default:
		;
	}

	return false;
}

template<typename T>
bool AstBinaryOp::ApplyConstBinaryOp(Int &bres, T &res, const T &v0, const T &v1) const
{
	switch(type)
	{
	case AST_OP_ADD:
		res = v0 + v1;
		break;

	case AST_OP_SUB:
		res = v0 - v1;
		break;

	case AST_OP_MUL:
		res = v0 * v1;
		break;

	case AST_OP_DIV:
		LETHE_RET_FALSE(v1);
		res = v0 / v1;
		break;

	case AST_OP_MOD:
		LETHE_RET_FALSE(v1);
		res = v0 % v1;
		break;

	case AST_OP_AND:
		res = v0 & v1;
		break;

	case AST_OP_OR:
		res = v0 | v1;
		break;

	case AST_OP_XOR:
		res = v0 ^ v1;
		break;

	case AST_OP_SHL:
		res = v0 << (v1 & (8*sizeof(T)-1));
		break;

	case AST_OP_SHR:
		res = v0 >> (v1 & (8*sizeof(T)-1));
		break;

	case AST_OP_EQ:
		bres = v0 == v1;
		break;

	case AST_OP_NEQ:
		bres = v0 != v1;
		break;

	case AST_OP_LT:
		bres = v0 < v1;
		break;

	case AST_OP_LEQ:
		bres = v0 <= v1;
		break;

	case AST_OP_GT:
		bres = v0 > v1;
		break;

	case AST_OP_GEQ:
		bres = v0 >= v1;
		break;

	default:
		return 0;
	}

	return 1;
}

template<typename T>
bool AstBinaryOp::ApplyConstBinaryOpFloat(Int &bres, T &res, const T &v0, const T &v1) const
{
	switch(type)
	{
	case AST_OP_ADD:
		res = v0 + v1;
		break;

	case AST_OP_SUB:
		res = v0 - v1;
		break;

	case AST_OP_MUL:
		res = v0 * v1;
		break;

	case AST_OP_DIV:
		LETHE_RET_FALSE(v1);
		res = v0 / v1;
		break;

	case AST_OP_EQ:
		bres = v0 == v1;
		break;

	case AST_OP_NEQ:
		bres = v0 != v1;
		break;

	case AST_OP_LT:
		bres = v0 < v1;
		break;

	case AST_OP_LEQ:
		bres = v0 <= v1;
		break;

	case AST_OP_GT:
		bres = v0 > v1;
		break;

	case AST_OP_GEQ:
		bres = v0 >= v1;
		break;

	default:
		return false;
	}

	return true;
}

bool AstBinaryOp::FoldConst(const CompiledProgram &p)
{
	if (!nodes[0]->IsConstant() || !nodes[1]->IsConstant())
		return Super::FoldConst(p);

	LETHE_ASSERT(parent);

	const QDataType &t0 = nodes[0]->GetTypeDesc(p);
	const QDataType &t1 = nodes[1]->GetTypeDesc(p);
	DataTypeEnum tdt = DataType::ComposeTypeEnum(t0.GetTypeEnum(), t1.GetTypeEnum());

	// convert nodes to type
	LETHE_RET_FALSE(nodes[0]->ConvertConstTo(tdt, p));
	LETHE_RET_FALSE(nodes[1]->ConvertConstTo(tdt, p));

	// handle relational ops...
	const bool isCmp = IsCompare();

	UniquePtr<AstNode> res = AstCreateConstNode(isCmp ? DT_BOOL : tdt, location);

	// ok now apply operator
	switch(tdt)
	{
	case DT_BOOL:
	case DT_BYTE:
	case DT_SBYTE:
	case DT_USHORT:
	case DT_SHORT:
	case DT_INT:
		LETHE_RET_FALSE(ApplyConstBinaryOp<Int>(res->num.i, res->num.i, nodes[0]->num.i, nodes[1]->num.i));
		break;

	case DT_UINT:
		LETHE_RET_FALSE(ApplyConstBinaryOp<UInt>(res->num.i, res->num.ui, nodes[0]->num.ui, nodes[1]->num.ui));
		break;

	case DT_LONG:
		LETHE_RET_FALSE(ApplyConstBinaryOp<Long>(res->num.i, res->num.l, nodes[0]->num.l, nodes[1]->num.l));
		break;

	case DT_ULONG:
		LETHE_RET_FALSE(ApplyConstBinaryOp<ULong>(res->num.i, res->num.ul, nodes[0]->num.ul, nodes[1]->num.ul));
		break;

	case DT_FLOAT:
		LETHE_RET_FALSE(ApplyConstBinaryOpFloat<Float>(res->num.i, res->num.f, nodes[0]->num.f, nodes[1]->num.f));
		break;

	case DT_DOUBLE:
		LETHE_RET_FALSE(ApplyConstBinaryOpFloat<Double>(res->num.i, res->num.d, nodes[0]->num.d, nodes[1]->num.d));
		break;

	case DT_NAME:
		LETHE_ASSERT(0 && "cannot coerce to name!");
		return false;

	case DT_STRING:
	{
		const auto &v0 = AstStaticCast<const AstText *>(nodes[0])->text;
		const auto &v1 = AstStaticCast<const AstText *>(nodes[1])->text;

		if (type == AST_OP_EQ)
		{
			res->num.i = v0 == v1;
			break;
		}

		if (type == AST_OP_NEQ)
		{
			res->num.i = v0 != v1;
			break;
		}

		LETHE_RET_FALSE(type == AST_OP_ADD);
		AstStaticCast<AstText *>(res.Get())->text = v0 + v1;
		break;
	}

	default:
		;
	}

	parent->ReplaceChild(this, res.Detach());
	delete this;

	return true;
}

QDataType AstBinaryOp::GetTypeDesc(const CompiledProgram &p) const
{
	// get types for subtrees
	const auto &dt0 = nodes[0]->GetTypeDesc(p);
	const auto &dt1 = nodes[1]->GetTypeDesc(p);

	switch(type)
	{
	case AST_OP_LT:
	case AST_OP_LEQ:
	case AST_OP_GT:
	case AST_OP_GEQ:
	case AST_OP_EQ:
	case AST_OP_NEQ:
		if (!dt0.IsStruct() && !dt1.IsStruct())
			return QDataType::MakeType(p.elemTypes[DT_BOOL]);

	default:
		;
	}

	// FIXME: better!!!
	if (type == AST_OP_ASSIGN)
	{
		if (dt0.GetTypeEnum() == DT_ARRAY_REF || dt0.GetTypeEnum() == DT_DYNAMIC_ARRAY)
			return dt0;

		if (dt0.IsStruct() && dt1.IsStruct() && dt0.CanAssign(dt1))
			return dt0;
	}

	if (dt0.IsStruct() || dt1.IsStruct())
	{
		auto *opName = GetOpName();

		AstNode *op = nullptr;

		if (dt0.IsStruct())
		{
			auto scope = dt0.GetType().structScopeRef;

			if (scope)
				op = scope->FindOperator(p, opName, dt0, dt1);
		}

		if (dt1.IsStruct() && &dt0.GetType() != &dt1.GetType())
		{
			auto scope = dt1.GetType().structScopeRef;

			AstNode *nop = scope ? scope->FindOperator(p, opName, dt0, dt1) : nullptr;

			if (!op)
				op = nop;
			else if (nop)
			{
				// ambiguous
				op = nullptr;
			}
		}

		if (op)
			return op->nodes[AstFunc::IDX_RET]->GetTypeDesc(p);
	}

	return QDataType::MakeType(
			   p.elemTypes[DataType::ComposeTypeEnum(dt0.GetTypeEnum(), dt1.GetTypeEnum())]);
}

bool AstBinaryOp::IsCommutative(const CompiledProgram &p) const
{
	// note: logical and and or are technically commutative,
	// but since we use this for optimization (reordering),
	// we can't reorder && and ||, we rely on the programmer

	// note: this optimization cannot be used for strings and custom operators!
	switch(type)
	{
	case AST_OP_ADD:
	case AST_OP_MUL:
	case AST_OP_AND:
	case AST_OP_OR:
	case AST_OP_XOR:
	case AST_OP_EQ:
	case AST_OP_NEQ:
		return nodes[IDX_LEFT]->GetTypeDesc(p).IsNumber() && nodes[IDX_RIGHT]->GetTypeDesc(p).IsNumber();

	default:
		;
	}

	return false;
}

namespace
{

Int OpcodeAdd(DataTypeEnum dt)
{
	if (dt >= DT_BOOL && dt <= DT_UINT)
		return OPC_IADD;

	switch(dt)
	{
	case DT_LONG:
	case DT_ULONG:
		return OPC_LADD;

	case DT_FLOAT:
		return OPC_FADD;

	case DT_DOUBLE:
		return OPC_DADD;

	case DT_STRING:
		return OPC_BCALL + (BUILTIN_LSTRADD << 8);

	default:
		;
	}

	return OPC_HALT;
}

Int OpcodeSub(DataTypeEnum dt)
{
	if (dt >= DT_BOOL && dt <= DT_UINT)
		return OPC_ISUB;

	switch(dt)
	{
	case DT_LONG:
	case DT_ULONG:
		return OPC_LSUB;

	case DT_FLOAT:
		return OPC_FSUB;

	case DT_DOUBLE:
		return OPC_DSUB;

	default:
		;
	}

	return OPC_HALT;
}

Int OpcodeMul(DataTypeEnum dt)
{
	if (dt >= DT_BOOL && dt <= DT_UINT)
		return OPC_IMUL;

	switch(dt)
	{
	case DT_LONG:
	case DT_ULONG:
		return OPC_LMUL;

	case DT_FLOAT:
		return OPC_FMUL;

	case DT_DOUBLE:
		return OPC_DMUL;

	default:
		;
	}

	return OPC_HALT;
}

Int OpcodeMod(DataTypeEnum dt)
{
	if (dt >= DT_BOOL && dt < DT_UINT)
		return OPC_IMOD;

	switch (dt)
	{
	case DT_UINT:
		return OPC_UIMOD;

	case DT_LONG:
		return OPC_LMOD;

	case DT_ULONG:
		return OPC_ULMOD;

	case DT_FLOAT:
		return OPC_BCALL + (BUILTIN_FMOD << 8);

	case DT_DOUBLE:
		return OPC_BCALL + (BUILTIN_DMOD << 8);

	default:
		;
	}

	return OPC_HALT;
}

Int OpcodeDiv(DataTypeEnum dt)
{
	if (dt >= DT_BOOL && dt < DT_UINT)
		return OPC_IDIV;

	switch(dt)
	{
	case DT_UINT:
		return OPC_UIDIV;

	case DT_LONG:
		return OPC_LDIV;

	case DT_ULONG:
		return OPC_ULDIV;

	case DT_FLOAT:
		return OPC_FDIV;

	case DT_DOUBLE:
		return OPC_DDIV;

	default:
		;
	}

	return OPC_HALT;
}

Int OpcodeShr(DataTypeEnum dt)
{
	if (dt >= DT_BOOL && dt <= DT_UINT)
		return dt == DT_UINT ? OPC_ISHR : OPC_ISAR;

	if (dt == DT_LONG)
		return OPC_LSAR;

	if (dt == DT_ULONG)
		return OPC_LSHR;

	return OPC_HALT;
}

Int OpcodeIntBinary(DataTypeEnum dt, Int iopc, Int lopc)
{
	if (dt >= DT_BOOL && dt <= DT_UINT)
		return iopc;

	if (dt == DT_LONG || dt == DT_ULONG)
		return lopc;

	return OPC_HALT;
}

template< Int opci, Int opcui, Int opcl, Int opcul, Int opcf, Int opcd, Int opcs >
Int OpCodeCmp(DataTypeEnum dt)
{
	if ((dt >= DT_BOOL && dt < DT_UINT) || dt == DT_NAME)
		return opci;

	switch(dt)
	{
	case DT_UINT:
		return opcui;

	case DT_LONG:
		return opcl;

	case DT_ULONG:
		return opcul;

	case DT_FLOAT:
		return opcf;

	case DT_DOUBLE:
		return opcd;

	case DT_STRING:
		return opcs;

	default:
		;
	}

	return OPC_HALT;
}

Int OpCodeEq(DataTypeEnum dt)
{
	return OpCodeCmp< OPC_ICMPEQ, OPC_ICMPEQ, OPC_LCMPEQ, OPC_LCMPEQ, OPC_FCMPEQ, OPC_DCMPEQ, OPC_BCALL + (BUILTIN_SCMPEQ << 8) >(dt);
}

Int OpCodeNe(DataTypeEnum dt)
{
	return OpCodeCmp< OPC_ICMPNE, OPC_ICMPNE, OPC_LCMPNE, OPC_LCMPNE, OPC_FCMPNE, OPC_DCMPNE, OPC_BCALL + (BUILTIN_SCMPNE << 8) >(dt);
}

Int OpCodeLt(DataTypeEnum dt)
{
	return OpCodeCmp< OPC_ICMPLT, OPC_UICMPLT, OPC_LCMPLT, OPC_ULCMPLT, OPC_FCMPLT, OPC_DCMPLT, OPC_BCALL + (BUILTIN_SCMPLT << 8) >(dt);
}

Int OpCodeLe(DataTypeEnum dt)
{
	return OpCodeCmp< OPC_ICMPLE, OPC_UICMPLE, OPC_LCMPLE, OPC_ULCMPLE, OPC_FCMPLE, OPC_DCMPLE, OPC_BCALL + (BUILTIN_SCMPLE << 8) >(dt);
}

Int OpCodeGt(DataTypeEnum dt)
{
	return OpCodeCmp< OPC_ICMPGT, OPC_UICMPGT, OPC_LCMPGT, OPC_ULCMPGT, OPC_FCMPGT, OPC_DCMPGT, OPC_BCALL + (BUILTIN_SCMPGT << 8) >(dt);
}

Int OpCodeGe(DataTypeEnum dt)
{
	return OpCodeCmp< OPC_ICMPGE, OPC_UICMPGE, OPC_LCMPGE, OPC_ULCMPGE, OPC_FCMPGE, OPC_DCMPGE, OPC_BCALL + (BUILTIN_SCMPGE << 8) >(dt);
}

}

const char *AstBinaryOp::GetOpName() const
{
	const char *opName = nullptr;

	switch (type)
	{
	case AST_OP_ADD:
	case AST_OP_ADD_ASSIGN:
		opName = "+";
		break;

	case AST_OP_SUB:
	case AST_OP_SUB_ASSIGN:
		opName = "-";
		break;

	case AST_OP_MUL:
	case AST_OP_MUL_ASSIGN:
		opName = "*";
		break;

	case AST_OP_DIV:
	case AST_OP_DIV_ASSIGN:
		opName = "/";
		break;

	case AST_OP_MOD:
	case AST_OP_MOD_ASSIGN:
		opName = "%";
		break;

	case AST_OP_AND:
	case AST_OP_AND_ASSIGN:
		opName = "&";
		break;

	case AST_OP_OR:
	case AST_OP_OR_ASSIGN:
		opName = "|";
		break;

	case AST_OP_XOR:
	case AST_OP_XOR_ASSIGN:
		opName = "^";
		break;

	case AST_OP_SHL:
	case AST_OP_SHL_ASSIGN:
		opName = "<<";
		break;

	case AST_OP_SHR:
	case AST_OP_SHR_ASSIGN:
		opName = ">>";
		break;

	case AST_OP_EQ:
		opName = "==";
		break;

	case AST_OP_NEQ:
		opName = "!=";
		break;

	case AST_OP_LT:
		opName = "<";
		break;

	case AST_OP_LEQ:
		opName = "<=";
		break;

	case AST_OP_GT:
		opName = ">";
		break;

	case AST_OP_GEQ:
		opName = ">=";
		break;

	default:
		;
	}

	return opName;
}

bool AstBinaryOp::CodeGenOperator(CompiledProgram &p)
{
	auto left = nodes[IDX_LEFT]->GetTypeDesc(p);
	auto right = nodes[IDX_RIGHT]->GetTypeDesc(p);

	AstNode *op = nullptr;

	auto opName = GetOpName();

	if (!opName)
		return p.Error(this, "invalid operator type");

	if (left.IsStruct())
	{
		auto *scope = left.GetType().structScopeRef;

		if (!scope)
			return p.Error(this, "no struct scope for lhs");

		op = scope->FindOperator(p, opName, left, right);
	}

	if (right.IsStruct() && &left.GetType() != &right.GetType())
	{
		auto *scope = right.GetType().structScopeRef;

		if (!scope)
			return p.Error(this, "no struct scope for rhs");

		auto nop = scope->FindOperator(p, opName, left, right);

		if (!op)
			op = nop;
		else if (nop)
			return p.Error(this, "ambiguous operator");
	}

	if (!op)
		return p.Error(this, "matching operator not found!");

	return AstCall::CallOperator(2, p, this, op);
}

bool AstBinaryOp::CodeGenRef(CompiledProgram &p, bool allowConst, bool derefPtr)
{
	(void)allowConst;
	(void)derefPtr;
	return CodeGenCommon(p, true);
}

bool AstBinaryOp::CodeGen(CompiledProgram &p)
{
	return CodeGenCommon(p);
}

bool AstBinaryOp::CodeGenCommon(CompiledProgram &p, bool asRef)
{
	if (IsCommutative(p))
	{
		if ((nodes[0]->IsConstant() && !nodes[1]->IsConstant()) || (nodes[0]->GetDepth() < nodes[1]->GetDepth()))
		{
			// reorder because opcode optimizer is more efficient for things like i+1 instead of 1+i
			Swap(nodes[0], nodes[1]);
		}
	}

	// apply binary op on top of program exprStack
	// FIXME: very crude for testing; note that we must convert both to compatible types;
	// this is a problem with stack...
	// say we want to use long + int =>
	// stack layout (growing down):
	//		long (2 words)
	//		int  (1 word)
	// solution: do conversion before pushing; so if parent op is binary then convert to common types!
	// however, this might not be true for shift ops where we actually want to shift by a smaller amount...

	const auto leftType = nodes[0]->GetTypeDesc(p);
	const auto rightType = nodes[1]->GetTypeDesc(p);

	if (leftType.IsStruct() || rightType.IsStruct())
		return CodeGenOperator(p);

	const DataType &dtdst = asRef ? leftType.GetType() : p.Coerce(leftType.GetType(), rightType.GetType());
	const auto *dtdstr = &dtdst;
	DataTypeEnum dt = dtdst.type;
	auto dtr = dt;

	if (IsShift() && leftType.IsLongInt())
	{
		dtr = DT_UINT;
		dtdstr = &p.elemTypes[DT_UINT];
	}

	if (!nodes[0]->ConvertConstTo(dt, p))
		return p.Error(nodes[0], "cannot convert constant");

	if (!nodes[1]->ConvertConstTo(dtr, p))
		return p.Error(nodes[1], "cannot convert constant");

	auto dt0 = nodes[0]->GetTypeDesc(p);
	auto dt1 = nodes[1]->GetTypeDesc(p);

	LETHE_RET_FALSE(asRef ? nodes[0]->CodeGenRef(p) : nodes[0]->CodeGen(p));

	if (p.exprStack.IsEmpty())
		return p.Error(this, "lhs must return a value");

	dt0 = p.exprStack.Back();

	if (asRef)
	{
		p.Emit(OPC_LPUSHPTR);

		LETHE_RET_FALSE(EmitPtrLoad(leftType, p));
	}

	if (const auto *dt0enum = dt0.GetEnumType())
		if (const auto *dt1enum = dt1.GetEnumType())
			if (dt0enum != dt1enum)
				return p.Error(this, "incompatible enum types");

	if (dt0.GetTypeEnum() != dt)
	{
		// convert
		LETHE_RET_FALSE(p.EmitConv(nodes[0], dt0, dtdst, 0));
	}

	LETHE_RET_FALSE(nodes[1]->CodeGen(p));

	if (p.exprStack.IsEmpty())
		return p.Error(this, "rhs must return a value");

	dt1 = p.exprStack.Back();

	if (dt1.GetTypeEnum() != dtr)
	{
		// convert
		LETHE_RET_FALSE(p.EmitConv(nodes[1], dt1, *dtdstr, 0));
	}

	LETHE_ASSERT(p.exprStack.GetSize() >= 2);

	bool eqCmp = type == AST_OP_EQ || type == AST_OP_NEQ;

	if (eqCmp && dtdst.type == DT_DELEGATE)
	{
		// special handling for delegates!
		p.EmitI24(OPC_BCALL, type == AST_OP_EQ ? BUILTIN_CMPDG_EQ : BUILTIN_CMPDG_NE);
		p.PopStackType(1);
		p.PopStackType(1);
		p.PushStackType(GetTypeDesc(p));
		return true;
	}

	if (eqCmp && (dtdst.IsPointer() || dtdst.type == DT_FUNC_PTR))
	{
		// special handling for pointers!
		const Int opcmp[2] = { OPC_PCMPEQ, OPC_PCMPNE };
		p.Emit(opcmp[type == AST_OP_NEQ]);

		if (dt0.HasDtor())
		{
			p.EmitI24(OPC_LPUSHADR, 2);
			p.EmitBackwardJump(OPC_CALL, dt0.GetType().funDtor);
			p.EmitI24(OPC_POP, 1);
		}

		if (dt1.HasDtor())
		{
			p.EmitI24(OPC_LPUSHADR, 1);
			p.EmitBackwardJump(OPC_CALL, dt1.GetType().funDtor);
			p.EmitI24(OPC_POP, 1);
		}

		p.Emit(OPC_LMOVEPTR + (2 << 8) + (0 << 16));
		p.EmitI24(OPC_POP, 2);

		p.PopStackType(1);
		p.PopStackType(1);
		p.PushStackType(GetTypeDesc(p));
		return true;
	}

	Int opc = OPC_HALT;

	switch(type)
	{
	case AST_OP_ADD:
	case AST_OP_ADD_ASSIGN:
		opc = OpcodeAdd(dt);
		break;

	case AST_OP_SUB:
	case AST_OP_SUB_ASSIGN:
		opc = OpcodeSub(dt);
		break;

	case AST_OP_MUL:
	case AST_OP_MUL_ASSIGN:
		opc = OpcodeMul(dt);
		break;

	case AST_OP_DIV:
	case AST_OP_DIV_ASSIGN:
		opc = OpcodeDiv(dt);
		break;

	case AST_OP_MOD:
	case AST_OP_MOD_ASSIGN:
		opc = OpcodeMod(dt);
		break;

	case AST_OP_AND:
	case AST_OP_AND_ASSIGN:
		opc = OpcodeIntBinary(dt, OPC_IAND, OPC_LAND);
		break;

	case AST_OP_OR:
	case AST_OP_OR_ASSIGN:
		opc = OpcodeIntBinary(dt, OPC_IOR, OPC_LOR);
		break;

	case AST_OP_XOR:
	case AST_OP_XOR_ASSIGN:
		opc = OpcodeIntBinary(dt, OPC_IXOR, OPC_LXOR);
		break;

	case AST_OP_SHL:
	case AST_OP_SHL_ASSIGN:
		opc = OpcodeIntBinary(dt, OPC_ISHL, OPC_LSHL);
		break;

	case AST_OP_SHR:
	case AST_OP_SHR_ASSIGN:
		opc = OpcodeShr(dt);
		break;

	case AST_OP_EQ:
		opc = OpCodeEq(dt);
		break;

	case AST_OP_NEQ:
		opc = OpCodeNe(dt);
		break;

	case AST_OP_LT:
		opc = OpCodeLt(dt);
		break;

	case AST_OP_LEQ:
		opc = OpCodeLe(dt);
		break;

	case AST_OP_GT:
		opc = OpCodeGt(dt);
		break;

	case AST_OP_GEQ:
		opc = OpCodeGe(dt);
		break;

	default:
		;
	}

	if (opc == OPC_HALT)
		return p.Error(this, "invalid operator for specified type");

	p.Emit(opc);
	p.PopStackType(1);
	p.PopStackType(1);
	p.PushStackType(GetTypeDesc(p));
	return true;
}

const AstNode *AstBinaryOp::FindUserDefOperatorType(const char *opName, const AstNode *type0, const AstNode *type1)
{
	LETHE_RET_FALSE(type0 && type1);

	for (Int i=0; i<2; i++)
	{
		auto *tpe = i == 0 ? type0 : type1;

		if (tpe->type != AST_STRUCT || !tpe->scopeRef)
			continue;

		for (auto &&it : tpe->scopeRef->operators)
		{
			if (it->type != AST_FUNC)
				continue;

			auto *fn = AstStaticCast<AstFunc *>(it);

			const auto &fname = AstStaticCast<AstText *>(fn->nodes[AstFunc::IDX_NAME])->text;

			// operator name doesn't match => ignore this one
			if (fname != opName)
				continue;

			auto *args = fn->GetArgs();

			// try to match arguments...
			if (!args || args->nodes.GetSize() != 2)
				continue;

			auto *arg0Type = args->nodes[0]->GetTypeNode();

			if (!arg0Type || arg0Type->type != type0->type)
				continue;

			auto *arg1Type = args->nodes[1]->GetTypeNode();

			if (!arg1Type || arg1Type->type != type1->type)
				continue;

			// still need to match structs exactly
			if (arg0Type->type == AST_STRUCT && arg0Type != type0)
				continue;

			if (arg1Type->type == AST_STRUCT && arg1Type != type1)
				continue;

			// match found
			return fn->GetResult()->GetTypeNode();
		}
	}

	return nullptr;
}

const AstNode *AstBinaryOp::FindUserDefOperatorType(const AstNode *type0, const AstNode *type1) const
{
	return FindUserDefOperatorType(GetOpName(), type0, type1);
}

bool AstBinaryOp::ReturnsBool() const
{
	switch(type)
	{
	case AST_OP_EQ:
	case AST_OP_NEQ:
	case AST_OP_LEQ:
	case AST_OP_GEQ:
	case AST_OP_LT:
	case AST_OP_GT:
	// note: logical or and and handled elsewhere
		return true;

	default:;
	}

	return false;
}

const AstNode *AstBinaryOp::GetTypeNode() const
{
	// TODO: handle reference types, DAMN! (really?!)
	const auto *type0 = nodes[0]->GetTypeNode();
	const auto *type1 = nodes[1]->GetTypeNode();

	if (type0 && type1 && (type0->type == AST_STRUCT || type1->type == AST_STRUCT))
	{
		// try user-defined operators...
		return FindUserDefOperatorType(type0, type1);
	}

	// relational operators should return bool type!
	if (ReturnsBool())
		return &boolType;

	const auto *ctype = CoerceTypes(type0, type1);

	return ctype ? ctype : type0;
}


}
