#include "AstVarDeclList.h"
#include "../AstText.h"
#include "../AstSymbol.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

LETHE_AST_BUCKET_ALLOC_DEF(AstVarDeclList)

// AstVarDeclList

void AstVarDeclList::LoadIfVarDecl(CompiledProgram &p)
{
	if (nodes.GetSize() < 2 || !nodes[1])
		return;

	// problem: it's not a symbol
	AstText *sname = reinterpret_cast<AstText *>(nodes[1]->nodes[0]);
	AstSymbol sym(sname->text, sname->location);
	sym.target = nodes[1];
	sym.target->flags |= AST_F_REFERENCED;
	sym.scopeRef = scopeRef;
	sym.CodeGen(p);
}


}
