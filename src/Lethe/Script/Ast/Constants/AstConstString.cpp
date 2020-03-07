#include "AstConstString.h"
#include <Lethe/Script/Ast/Types/AstTypeString.h>
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstConstString

QDataType AstConstString::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_STRING];
	res.qualifiers = AST_Q_CONST;
	return res;
}

bool AstConstString::CodeGen(CompiledProgram &p)
{
	UInt ofs = (UInt)p.cpool.Add(text);

	p.EmitI24(OPC_PUSH_ICONST, ofs);
	p.Emit(OPC_BCALL + (BUILTIN_LPUSHSTR_CONST << 8));
	p.PushStackType(GetTypeDesc(p));
	return true;
}

bool AstConstString::CodeGenRef(CompiledProgram &p, bool allowConst, bool)
{
	if (!allowConst)
		return Super::CodeGenRef(p, allowConst);

	UInt ofs = (UInt)p.cpool.Add(text);

	p.EmitI24(OPC_PUSH_ICONST, ofs);
	p.Emit(OPC_BCALL + (BUILTIN_APUSHSTR_CONST << 8));

	auto qdt = GetTypeDesc(p);
	qdt.qualifiers |= AST_Q_REFERENCE;
	p.PushStackType(qdt);

	return true;
}

const AstNode *AstConstString::GetTypeNode() const
{
	static AstTypeString tnode{TokenLocation()};
	return &tnode;
}


}
