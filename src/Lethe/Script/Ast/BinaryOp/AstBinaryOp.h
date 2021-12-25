#pragma once

#include <Lethe/Script/Ast/AstNode.h>
#include <Lethe/Script/Ast/Types/AstTypeBool.h>

namespace lethe
{

class LETHE_API AstBinaryOp : public AstNode
{
public:
	LETHE_AST_NODE(AstBinaryOp)

	enum
	{
		IDX_LEFT,
		IDX_RIGHT
	};

	typedef AstNode Super;
	AstBinaryOp(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc) {}

	bool FoldConst(const CompiledProgram &p) override;
	bool CodeGen(CompiledProgram &p) override;
	bool CodeGenRef(CompiledProgram &p, bool allowConst = false, bool derefPtr = false) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool IsCompare() const;
	bool IsShift() const;
	bool IsShiftOrDiv() const;

	const AstNode *GetTypeNode() const override;

	const AstNode *GetContextTypeNode(const AstNode *node) const override;

	static const AstNode *FindUserDefOperatorType(const char *opName, const AstNode *type0, const AstNode *type1);

protected:
	// 1 = yes, 0 = no, -1 = force swap
	Int IsCommutative(const CompiledProgram &p) const;

	bool CodeGenOperator(CompiledProgram &p);
	bool CodeGenCommon(CompiledProgram &p, bool asRef = false);

	template<typename T, typename L>
	bool ApplyConstBinaryOp(Int &bres, T &res, const T &v0, const T &v1, const char *&warn) const;
	template<typename T>
	bool ApplyConstBinaryOpFloat(Int &bres, T &res, const T &v0, const T &v1) const;

private:
	const char *GetOpName() const;
	const AstNode *FindUserDefOperatorType(const AstNode *type0, const AstNode *type1) const;

	bool ReturnsBool() const;

	void CheckWarn(const DataType &ldt, const CompiledProgram &p, const AstNode *n);

	static bool SmallIntNeverNegative(DataTypeEnum dte);

	void CmpWarn(const CompiledProgram &p, const QDataType &left, const QDataType &right, DataTypeEnum dste);

	static AstTypeBool boolType;
};

}
