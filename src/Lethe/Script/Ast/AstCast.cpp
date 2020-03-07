#include "AstCast.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstCast

bool AstCast::FoldConst(const CompiledProgram &p)
{
	bool res = Super::FoldConst(p);

	if (!nodes[1]->IsConstant())
		return res;

	LETHE_RET_FALSE(nodes[1]->ConvertConstTo(GetTypeDesc(p).GetTypeEnum(), p));
	return res;
}

QDataType AstCast::GetTypeDesc(const CompiledProgram &p) const
{
	auto res = nodes[0]->GetTypeDesc(p);
	// avoid leaks in cast ... new/call
	FixPointerQualifiers(res, nodes[1]);
	return res;
}

bool AstCast::CodeGen(CompiledProgram &p)
{
	if (nodes.GetSize() != 2)
		return p.Error(this, "invalid cast (parser)");

	LETHE_RET_FALSE(nodes[1]->CodeGen(p));

	if (p.exprStack.IsEmpty())
		return p.Error(this, "cast expression must return a value");

	auto src = nodes[1]->GetTypeDesc(p);
	const QDataType &dst = GetTypeDesc(p);

	if (dst.IsArray() && dst.GetTypeEnum() != DT_ARRAY_REF)
		return p.Error(this, "cannot cast to array type");

	// avoid leaks in cast ... new/call
	FixPointerQualifiers(src, nodes[1]);

	LETHE_RET_FALSE(p.EmitConv(this, src, dst.GetType(), false));

	// TODO: this is suboptiomal and could be peephole optimized if last opcode is load
	if (src.GetTypeEnum() != dst.GetTypeEnum())
	{
		switch(dst.GetTypeEnum())
		{
		case DT_SBYTE:
			p.Emit(OPC_CONV_ITOSB);
			break;

		case DT_SHORT:
			p.Emit(OPC_CONV_ITOS);
			break;

		case DT_BYTE:
			p.EmitI24(OPC_IAND_ICONST, 0xff);
			break;

		case DT_USHORT:
			p.EmitI24(OPC_IAND_ICONST, 0xffff);
			break;

		default:
			;
		}
	}

	p.PopStackType(1);
	p.PushStackType(dst);
	return true;
}

const AstNode *AstCast::GetTypeNode() const
{
	return nodes[0]->GetTypeNode();
}


}
