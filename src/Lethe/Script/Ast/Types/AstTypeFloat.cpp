#include "AstTypeFloat.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeFloat

QDataType AstTypeFloat::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_FLOAT];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeFloat::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("float");
	return true;
}


}
