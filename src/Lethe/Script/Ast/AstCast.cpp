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

	nodes[1]->qualifiers |= AST_Q_NO_WARNINGS;
	auto dte = GetTypeDesc(p).GetTypeEnumUnderlying();

	if (dte == DT_NONE)
		return res;

	auto *oldn = nodes[1];
	auto *newn = nodes[1]->ConvertConstTo(dte, p);

	if (!newn || newn == oldn)
		return res;

	res = true;

	auto *n = nodes[1];
	nodes.Resize(1);

	LETHE_VERIFY(parent->ReplaceChild(this, n));
	delete this;
	return res;
}

QDataType AstCast::GetTypeDesc(const CompiledProgram &p) const
{
	auto res = nodes[0]->GetTypeDesc(p);
	// avoid leaks in cast ... new/call
	FixPointerQualifiers(p, res, nodes[1]);
	return res;
}

AstNode *AstCast::GetResolveTarget() const
{
	return nodes[0]->GetResolveTarget();
}

bool AstCast::CodeGen(CompiledProgram &p)
{
	if (nodes.GetSize() != 2)
		return p.Error(this, "invalid cast (parser)");

	bool isVoidCast = nodes[0]->type == AST_TYPE_VOID;

	// is it a simple cast without side effects?
	if (isVoidCast && !nodes[1]->HasSideEffects())
		return true;

	auto mark = p.ExprStackMark();

	LETHE_RET_FALSE(nodes[1]->CodeGen(p));

	if (!isVoidCast && p.exprStack.IsEmpty())
		return p.Error(this, "cast expression must return a value");

	auto dst = GetTypeDesc(p);

	if (dst.IsArray() && dst.GetTypeEnum() != DT_ARRAY_REF)
		return p.Error(this, "cannot cast to array type");

	auto src = nodes[1]->GetTypeDesc(p);

	// avoid leaks in cast ... new/call
	FixPointerQualifiers(p, src, nodes[1]);

	// we're allowing void casts now
	if (isVoidCast)
	{
		// void cast to clean up
		p.ExprStackCleanupTo(mark);
		return true;
	}

	LETHE_RET_FALSE(p.EmitConv(this, src, dst, false));

	// TODO: this is suboptimal and could be peephole optimized if last opcode is load
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
