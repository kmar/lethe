#include "AstProgram.h"

namespace lethe
{

// AstProgram

bool AstProgram::ResolveNode(const ErrorHandler &)
{
	if (nodes.IsEmpty())
		flags |= AST_F_RESOLVED;

	return 1;
}


}
