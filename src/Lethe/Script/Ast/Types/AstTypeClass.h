#pragma once

#include "AstTypeStruct.h"

namespace lethe
{

class LETHE_API AstTypeClass : public AstTypeStruct
{
	LETHE_BUCKET_ALLOC(AstTypeClass)

public:
	SCRIPT_AST_NODE(AstTypeClass)

	typedef AstTypeStruct Super;

	AstTypeClass(const TokenLocation &nloc);

	bool BeginCodegen(CompiledProgram &p) override;

	bool TypeGen(CompiledProgram &p) override;
	bool VtblGen(CompiledProgram &p) override;
	bool CodeGenComposite(CompiledProgram &p) override;

	QDataType GetTypeDescPtr(DataTypeEnum dte) const;

	Name GetName() const;

	// final mapping of virtual methods to vtable indices (used by "ignores" list)
	HashMap<String, Int> virtualMethods;

	// list of ignore virtual functions
	StackArray<String, 16> ignores;

	void CopyTo(AstNode *n) const override;

private:
	// special typeRef for pointers
	QDataType rawPtrTypeRef;
	QDataType ptrTypeRef;
	QDataType weakPtrTypeRef;
	Name className;
	// current vtbl index
	Int vtblIndex;
	// standard vtbl offset
	Int vtblOffset;

	bool VtblGenNestedClasses(CompiledProgram &p);
};


}
