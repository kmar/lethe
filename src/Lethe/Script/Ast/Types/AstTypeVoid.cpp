#include "AstTypeVoid.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeVoid

QDataType AstTypeVoid::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_NONE];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeVoid::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("void");
	return true;
}


}
