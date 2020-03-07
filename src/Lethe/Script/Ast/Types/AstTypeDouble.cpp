#include "AstTypeDouble.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeDouble

QDataType AstTypeDouble::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_DOUBLE];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeDouble::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("double");
	return true;
}


}
