#include "AstTextConstant.h"
#include "AstConstName.h"
#include "AstConstString.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTextConstant

AstNode *AstTextConstant::ConvertConstTo(DataTypeEnum dte, const CompiledProgram &p)
{
	if (dte != DT_STRING && dte != DT_NAME && dte != DT_ARRAY_REF)
	{
		p.Error(this, "cannot convert constant");
		return nullptr;
	}

	AstNode *conv = nullptr;

	if (type == AST_CONST_STRING)
	{
		if (dte == DT_NAME)
			conv = new AstConstName(text, location);
	}
	else
	{
		if (dte == DT_STRING)
			conv = new AstConstString(text, location);
	}

	if (conv)
	{
		conv->parent = parent;
		LETHE_VERIFY(parent->ReplaceChild(this, conv));
		delete this;
	}

	return conv ? conv : this;
}

Int AstTextConstant::ToBoolConstant(const CompiledProgram &)
{
	return !text.IsEmpty();
}

}
