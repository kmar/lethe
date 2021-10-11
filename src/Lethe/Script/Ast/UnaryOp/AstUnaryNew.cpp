#include "AstUnaryNew.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/Ast/CodeGenTables.h>
#include <Lethe/Script/Ast/Types/AstTypeClass.h>
#include <Lethe/Script/Ast/Types/AstTypeDef.h>

namespace lethe
{

// AstUnaryNew

const AstNode *AstUnaryNew::GetTypeNode() const
{
	return GetResolveTarget();
}

AstNode *AstUnaryNew::GetResolveTarget() const
{
	auto targ = nodes[IDX_CLASS]->target;

	if (!targ || targ->type != AST_CLASS)
	{
		// TODO: find object!
		return nullptr;
	}

	return nodes[IDX_CLASS]->GetResolveTarget();
}

QDataType AstUnaryNew::GetTypeDesc(const CompiledProgram &p) const
{
	auto *targ = nodes[IDX_CLASS]->target;

	while (targ && targ->type == AST_TYPEDEF)
		targ = const_cast<AstNode *>(targ->GetTypeNode());

	if (!targ || targ->type != AST_CLASS)
	{
		auto ci = p.typeHash.Find("^object");
		auto res = ci != p.typeHash.End() ? QDataType::MakeType(*ci->value) : QDataType();
		res.qualifiers &= ~AST_Q_SKIP_DTOR;
		return res;
	}

	auto res = Super::GetTypeDesc(p);
	res.qualifiers &= ~AST_Q_SKIP_DTOR;
	return res;
}

bool AstUnaryNew::CodeGen(CompiledProgram &p)
{
	auto *targ = nodes[IDX_CLASS]->target;

	while (targ && targ->type == AST_TYPEDEF)
		targ = const_cast<AstNode *>(targ->GetTypeNode());

	bool isDynamic = false;

	if (!targ || targ->type != AST_CLASS)
	{
		isDynamic = true;

		auto dt = nodes[IDX_CLASS]->GetTypeDesc(p);
		auto dte = dt.GetTypeEnum();

		if (dte != DT_NAME && dte != DT_STRING)
			return p.Error(nodes[IDX_CLASS], "new requires class or name/string type");
	}

	// okay, so what do we do now? => simply push name (fully qualified class name)
	// and then builtin call new operator to push strong ptr on stack; actually we might need an instruction here...
	// => need to use name for each class; maybe even using local nametable for better type compatibility checking!

	auto ldt = GetTypeDesc(p);

	Name n;

	if (isDynamic)
	{
		LETHE_RET_FALSE(nodes[IDX_CLASS]->CodeGen(p));
		auto dt = p.exprStack.Back();

		if (dt.GetTypeEnum() == DT_STRING)
			LETHE_RET_FALSE(p.EmitConv(nodes[IDX_CLASS], dt, QDataType::MakeConstType(p.elemTypes[DT_NAME])));

		// pop because builtin_new will clean up automatically
		p.PopStackType(true);
		p.EmitI24(OPC_BCALL, BUILTIN_NEW_DYNAMIC);
		// new dynamic pushes ctor function pointer or null!

		p.Emit(OPC_PCMPNZ);
		// jump over call if null
		p.EmitI24(OPC_IBZ_P, 1);
		p.Emit(OPC_FCALL);
		p.EmitI24(OPC_POP, 1);
		p.EmitI24(OPC_BCALL, BUILTIN_ADD_STRONG_AFTER_NEW);
	}
	else
	{
		n = AstStaticCast<const AstTypeClass *>(targ)->GetName();
		p.EmitNameConst(n);
		p.Emit(OPC_BCALL + (BUILTIN_NEW << 8));

		p.EmitBackwardJump(OPC_CALL, ldt.GetType().elemType.GetType().funCtor);
		p.EmitI24(OPC_POP, 1);
		p.EmitI24(OPC_BCALL, BUILTIN_ADD_STRONG_AFTER_NEW);
	}

	p.PushStackType(ldt);
	return true;
}


}
