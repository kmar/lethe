#include "AstDefer.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include "NamedScope.h"

namespace lethe
{

// AstDefer

bool AstDefer::CodeGen(CompiledProgram &p)
{
	if (scopeRef->type == NSCOPE_FUNCTION && (scopeRef->node->qualifiers & AST_Q_INLINE) != 0)
		return p.Error(this, "defer statement cannot be used in inline functions");

	nodes[0]->flags |= AST_F_DEFER;
	return 1;
}


}
