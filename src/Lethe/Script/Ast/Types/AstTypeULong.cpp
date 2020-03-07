#include "AstTypeULong.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeULong

QDataType AstTypeULong::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_ULONG];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeULong::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("ulong");
	return true;
}


}
