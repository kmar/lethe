#include "AstTypeName.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeName

QDataType AstTypeName::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_NAME];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeName::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("name");
	return true;
}


}
