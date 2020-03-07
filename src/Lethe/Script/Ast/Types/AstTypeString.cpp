#include "AstTypeString.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeString

QDataType AstTypeString::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_STRING];
	res.qualifiers = qualifiers;
	return res;
}

bool AstTypeString::TypeGen(CompiledProgram &p)
{
	auto &dt = p.elemTypes[DT_STRING];

	if (dt.funDtor >= 0)
		return true;

	dt.name = "string";
	LETHE_RET_FALSE(dt.GenDtor(p));

	*p.types[dt.typeIndex] = dt;

	return true;
}

bool AstTypeString::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("string");
	return true;
}


}
