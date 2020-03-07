#include "AstTypeByte.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeByte

QDataType AstTypeByte::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_BYTE];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeByte::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("byte");
	return true;
}


}
