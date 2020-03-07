#include "AstTypeShort.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeShort

QDataType AstTypeShort::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_SHORT];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeShort::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("short");
	return true;
}


}
