#include "AstTypeSByte.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeSByte

QDataType AstTypeSByte::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_SBYTE];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeSByte::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("sbyte");
	return true;
}


}
