#include "AstConstant.h"
#include "AstConstBool.h"
#include "AstConstInt.h"
#include "AstConstChar.h"
#include "AstConstUInt.h"
#include "AstConstLong.h"
#include "AstConstULong.h"
#include "AstConstFloat.h"
#include "AstConstDouble.h"
#include "AstConstName.h"
#include "AstConstString.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Compiler/Warnings.h>

namespace lethe
{

// AstConstant

#define AST_CONST_CONV_WARN_IF(expr) do {if (dte != DT_BOOL && (expr) && !(qualifiers & AST_Q_NO_WARNINGS)) \
	p.Warning(this, "conversion loses precision", WARN_CONV_PRECISION);} while(false)

#define AST_CONST_CONV_TO(T, id, src, res, OP)	\
	switch(src) {	\
	case DT_BOOL:	\
		res->num.id = static_cast<T>(tmp.i OP);	\
		AST_CONST_CONV_WARN_IF((!!(res->num.id)) != tmp.i); \
		break; \
	case DT_SBYTE:	\
		res->num.id = static_cast<T>(tmp.i OP);	\
		AST_CONST_CONV_WARN_IF(static_cast<SByte>(res->num.id) != tmp.i); \
		break; \
	case DT_BYTE:	\
		res->num.id = static_cast<T>(tmp.i OP);	\
		AST_CONST_CONV_WARN_IF(static_cast<Byte>(res->num.id) != tmp.i); \
		break; \
	case DT_SHORT:	\
		res->num.id = static_cast<T>(tmp.i OP);	\
		AST_CONST_CONV_WARN_IF(static_cast<Short>(res->num.id) != tmp.i); \
		break; \
	case DT_USHORT:	\
		res->num.id = static_cast<T>(tmp.i OP);	\
		AST_CONST_CONV_WARN_IF(static_cast<UShort>(res->num.id) != tmp.i); \
		break; \
	case DT_CHAR:	\
	case DT_INT:	\
		res->num.id = static_cast<T>(tmp.i OP);	\
		AST_CONST_CONV_WARN_IF(static_cast<Int>(res->num.id) != tmp.i); \
		break;	\
	case DT_UINT:	\
		res->num.id = static_cast<T>(tmp.ui OP);	\
		AST_CONST_CONV_WARN_IF(static_cast<UInt>(res->num.id) != tmp.ui); \
		break;	\
	case DT_LONG:	\
		res->num.id = static_cast<T>(tmp.l OP);	\
		AST_CONST_CONV_WARN_IF(static_cast<Long>(res->num.id) != tmp.l); \
		break;	\
	case DT_ULONG:	\
		res->num.id = static_cast<T>(tmp.ul OP);	\
		AST_CONST_CONV_WARN_IF(static_cast<ULong>(res->num.id) != tmp.ul); \
		break;	\
	case DT_FLOAT:	\
		res->num.id = static_cast<T>(tmp.f OP);	\
		AST_CONST_CONV_WARN_IF(static_cast<Float>(res->num.id) != tmp.f); \
		break;	\
	case DT_DOUBLE:	\
		res->num.id = static_cast<T>(tmp.d OP);	\
		AST_CONST_CONV_WARN_IF(static_cast<Double>(res->num.id) != tmp.d); \
		break;	\
	default:;	\
	}

#define AST_CONST_CONV_TO_STRING(src, dres, res)	\
	switch(src) {	\
	case DT_CHAR:	\
	dres = res->num.i;	\
	break;	\
	case DT_BOOL:	\
	case DT_SBYTE:	\
	case DT_BYTE:	\
	case DT_SHORT:	\
	case DT_USHORT:	\
	case DT_INT:	\
	dres.Format("%d", res->num.i);	\
	break;	\
	case DT_UINT:	\
	dres.Format("%u", res->num.ui);	\
	break;	\
	case DT_LONG:	\
	dres.Format(LETHE_FORMAT_LONG, res->num.l);	\
	break;	\
	case DT_ULONG:	\
	dres.Format(LETHE_FORMAT_ULONG, res->num.ul);	\
	break;	\
	case DT_FLOAT:	\
	dres.Format("%f", res->num.f);	\
	break;	\
	case DT_DOUBLE:	\
	dres.Format("%lf", res->num.d);	\
	break;	\
	case DT_NAME:	\
    case DT_STRING:	\
	dres = reinterpret_cast<const AstText *>(res)->text;	\
	break;	\
	default:;	\
}

AstNode *AstNode::ConvertConstNode(const DataType &dt, DataTypeEnum dte, const CompiledProgram &p)
{
	AstNode *res = nullptr;

	auto tmp = num;

	switch(dte)
	{
	case DT_BOOL:
		res = new AstConstBool(location);
		AST_CONST_CONV_TO(bool, i, dt.type, res, !=0);
		break;

	case DT_BYTE:
		res = type == AST_CONST_INT ? this : new AstConstInt(location);
		AST_CONST_CONV_TO(Byte, i, dt.type, res,);
		break;

	case DT_SBYTE:
		res = type == AST_CONST_INT ? this : new AstConstInt(location);
		AST_CONST_CONV_TO(SByte, i, dt.type, res,);
		break;

	case DT_SHORT:
		res = type == AST_CONST_INT ? this : new AstConstInt(location);
		AST_CONST_CONV_TO(Short, i, dt.type, res,);
		break;

	case DT_USHORT:
		res = type == AST_CONST_INT ? this : new AstConstInt(location);
		AST_CONST_CONV_TO(UShort, i, dt.type, res,);
		break;

	case DT_CHAR:
		res = new AstConstChar(location);
		AST_CONST_CONV_TO(Int, i, dt.type, res,);
		break;

	case DT_ENUM:
	case DT_INT:
		res = new AstConstInt(location, dt.baseType.IsNumber() ? &dt : nullptr);
		AST_CONST_CONV_TO(Int, i, dt.type, res,);
		break;

	case DT_UINT:
		res = new AstConstUInt(location);
		AST_CONST_CONV_TO(UInt, ui, dt.type, res,);
		break;

	case DT_LONG:
		res = new AstConstLong(location);
		AST_CONST_CONV_TO(Long, l, dt.type, res,);
		break;

	case DT_ULONG:
		res = new AstConstULong(location);
		AST_CONST_CONV_TO(ULong, ul, dt.type, res,);
		break;

	case DT_FLOAT:
		res = new AstConstFloat(location);
		AST_CONST_CONV_TO(Float, f, dt.type, res,);
		break;

	case DT_DOUBLE:
		res = new AstConstDouble(location);
		AST_CONST_CONV_TO(Double, d, dt.type, res, );
		break;

	case DT_STRING:
		res = new AstConstString("", location);
		AST_CONST_CONV_TO_STRING(dt.type, AstStaticCast<AstText *>(res)->text, this);
		break;

	default:
		;
	}

	return res;
}

AstNode *AstConstant::ConvertConstTo(DataTypeEnum dte, const CompiledProgram &p)
{
	const DataType &dt = GetTypeDesc(p).GetType();
	auto thisType = dt.type;

	if (thisType == DT_ENUM)
		thisType = DT_INT;

	if (dte == thisType || !IsConstant())
		return this;

	// special handling for NULL
	if (dt.type == DT_NULL)
	{
		switch(dte)
		{
		case DT_FUNC_PTR:
		case DT_RAW_PTR:
		case DT_STRONG_PTR:
		case DT_WEAK_PTR:
		case DT_DELEGATE:
			return this;

		default:
			;
		}
	}

	// perform conversion...
	AstNode *res = ConvertConstNode(dt, dte, p);

	if (!res)
	{
		p.Error(this, String::Printf("cannot convert constant from %s to %s", dt.GetName().Ansi(), DataType::GetSimpleTypeName(dte).Ansi() ));
		return nullptr;
	}

	if (res != this)
	{
		res->parent = parent;
		LETHE_VERIFY(parent->ReplaceChild(this, res));
		delete this;
	}

	return res;
}

Int AstConstant::ToBoolConstant(const CompiledProgram &p)
{
	return ToBoolConstantInternal(p);
}

Int AstConstant::ToBoolConstantInternal(const CompiledProgram &p) const
{
	QDataType qdt = GetTypeDesc(p);
	DataTypeEnum dte = qdt.GetTypeEnum();

	switch(dte)
	{
	case DT_BOOL:
	case DT_BYTE:
	case DT_SBYTE:
	case DT_USHORT:
	case DT_SHORT:
	case DT_INT:
		return num.i != 0;

	case DT_UINT:
		return num.ui != 0;

	case DT_LONG:
		return num.l != 0;

	case DT_ULONG:
		return num.ul != 0;

	case DT_FLOAT:
		return num.f != 0.0f;

	case DT_DOUBLE:
		return num.d != 0.0;

	default:
		;
	}

	return -1;
}

bool AstConstant::IsZeroConstant(const CompiledProgram &p) const
{
	return AstConstant::ToBoolConstantInternal(p) == 0;
}

#undef AST_CONST_CONV_TO
#undef AST_CONST_CONV_TO_STRING
#undef AST_CONST_CONV_WARN_IF


}
