#include "AstTypeUInt.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeUInt

QDataType AstTypeUInt::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_UINT];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeUInt::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("uint");
	return true;
}


}
