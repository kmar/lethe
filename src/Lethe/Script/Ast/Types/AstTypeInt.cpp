#include "AstTypeInt.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeInt

QDataType AstTypeInt::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_INT];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeInt::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("int");
	return true;
}

}
