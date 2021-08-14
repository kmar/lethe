#include "AstWhile.h"
#include "AstFor.h"
#include "../NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstWhile

bool AstWhile::CodeGen(CompiledProgram &p)
{
	// [0] = cond, [1] = body, [2] = (opt)nobreak
	AstNode dummy(AST_EMPTY, TokenLocation());
	// convert into virtual for:
	// [0] = init, [1] = cond, [2] = inc, [3] = body, [4] = (opt) nobreak
	AstNode *tnodes[5] = {&dummy, nodes[0], &dummy, nodes[1], nodes.GetSize() > 2 ? nodes[2] : &dummy};
	ArrayRef<AstNode *> vnodes(tnodes, nodes.GetSize() > 2 ? 5 : 4);
	return AstFor::CodeGenCommon(scopeRef, p, vnodes);
}


}
