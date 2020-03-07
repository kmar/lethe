#include "AstNamespace.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstNamespace

bool AstNamespace::ResolveNode(const ErrorHandler &)
{
	if (nodes.IsEmpty())
		flags |= AST_F_RESOLVED;

	return 1;
}


}
