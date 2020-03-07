#include "AstTypeChar.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeChar

QDataType AstTypeChar::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_CHAR];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeChar::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("char");
	return true;
}


}
