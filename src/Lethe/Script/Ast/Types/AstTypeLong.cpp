#include "AstTypeLong.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeLong

QDataType AstTypeLong::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_LONG];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeLong::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("long");
	return true;
}


}
