#include "AstTypeUShort.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeUShort

QDataType AstTypeUShort::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_USHORT];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeUShort::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("ushort");
	return true;
}


}
