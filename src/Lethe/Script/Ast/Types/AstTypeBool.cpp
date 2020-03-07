#include "AstTypeBool.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeBool

QDataType AstTypeBool::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_BOOL];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeBool::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("bool");
	return true;
}


}
