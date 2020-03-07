#include "AstBaseType.h"

namespace lethe
{

// AstTypeBase

AstNode *AstBaseType::GetResolveTarget() const
{
	return const_cast<AstBaseType *>(this);
}


}
