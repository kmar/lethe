// inline

#include <Lethe/Script/Ast/BinaryOp/AstScopeResOp.h>
#include <Lethe/Script/Ast/AstText.h>

#include <Lethe/Script/Ast/Constants/AstConstBool.h>
#include <Lethe/Script/Ast/Constants/AstConstChar.h>
#include <Lethe/Script/Ast/Constants/AstConstInt.h>
#include <Lethe/Script/Ast/Constants/AstConstUInt.h>
#include <Lethe/Script/Ast/Constants/AstConstLong.h>
#include <Lethe/Script/Ast/Constants/AstConstULong.h>
#include <Lethe/Script/Ast/Constants/AstConstFloat.h>
#include <Lethe/Script/Ast/Constants/AstConstDouble.h>
#include <Lethe/Script/Ast/Constants/AstConstName.h>
#include <Lethe/Script/Ast/Constants/AstConstString.h>
#include <Lethe/Script/Ast/Constants/AstConstNull.h>
#include <Lethe/Script/Ast/Constants/AstEnumItem.h>

#include <Lethe/Script/Ast/ControlFlow/AstSwitch.h>
#include <Lethe/Script/Ast/ControlFlow/AstGoto.h>
#include <Lethe/Script/Ast/ControlFlow/AstLabel.h>
#include <Lethe/Script/Ast/ControlFlow/AstTernaryOp.h>
#include <Lethe/Script/Ast/ControlFlow/AstSwitchBody.h>
#include <Lethe/Script/Ast/ControlFlow/AstCase.h>
#include <Lethe/Script/Ast/ControlFlow/AstCaseDefault.h>
#include <Lethe/Script/Ast/ControlFlow/AstBreak.h>
#include <Lethe/Script/Ast/ControlFlow/AstContinue.h>
#include <Lethe/Script/Ast/ControlFlow/AstReturn.h>
#include <Lethe/Script/Ast/ControlFlow/AstDo.h>
#include <Lethe/Script/Ast/ControlFlow/AstWhile.h>
#include <Lethe/Script/Ast/ControlFlow/AstFor.h>
#include <Lethe/Script/Ast/ControlFlow/AstIf.h>

#include <Lethe/Script/Ast/AstNamespace.h>

#include <Lethe/Script/Ast/NamedScope.h>

#include <Lethe/Script/Ast/BinaryOp/AstBinaryOp.h>
#include <Lethe/Script/Ast/BinaryOp/AstLazyBinaryOp.h>
#include <Lethe/Script/Ast/BinaryOp/AstDotOp.h>
#include <Lethe/Script/Ast/BinaryOp/AstSubscriptOp.h>
#include <Lethe/Script/Ast/BinaryOp/AstBinaryAssign.h>
#include <Lethe/Script/Ast/BinaryOp/AstCommaOp.h>

#include <Lethe/Script/Ast/UnaryOp/AstUnaryPreOp.h>
#include <Lethe/Script/Ast/UnaryOp/AstUnaryPostOp.h>
#include <Lethe/Script/Ast/UnaryOp/AstUnaryPlus.h>
#include <Lethe/Script/Ast/UnaryOp/AstUnaryMinus.h>
#include <Lethe/Script/Ast/UnaryOp/AstUnaryNot.h>
#include <Lethe/Script/Ast/UnaryOp/AstUnaryLNot.h>
#include <Lethe/Script/Ast/UnaryOp/AstUnaryRef.h>
#include <Lethe/Script/Ast/UnaryOp/AstUnaryNew.h>
#include <Lethe/Script/Ast/UnaryOp/AstSizeOf.h>

#include <Lethe/Script/Ast/Function/AstCall.h>
#include <Lethe/Script/Ast/Function/AstFunc.h>
#include <Lethe/Script/Ast/Function/AstFuncBody.h>
#include <Lethe/Script/Ast/Function/AstArg.h>

#include <Lethe/Script/Ast/AstStructLiteral.h>
#include <Lethe/Script/Ast/AstThis.h>
#include <Lethe/Script/Ast/AstExpr.h>
#include <Lethe/Script/Ast/AstBlock.h>
#include <Lethe/Script/Ast/AstDefer.h>
#include <Lethe/Script/Ast/AstInitializerList.h>
#include <Lethe/Script/Ast/AstVarDecl.h>
#include <Lethe/Script/Ast/AstDefaultInit.h>
#include <Lethe/Script/Ast/AstProgram.h>

#include <Lethe/Script/Ast/AstCast.h>

#include <Lethe/Script/Ast/Types/AstVarDeclList.h>
#include <Lethe/Script/Ast/Types/AstTypeVoid.h>
#include <Lethe/Script/Ast/Types/AstTypeBool.h>
#include <Lethe/Script/Ast/Types/AstTypeByte.h>
#include <Lethe/Script/Ast/Types/AstTypeSByte.h>
#include <Lethe/Script/Ast/Types/AstTypeShort.h>
#include <Lethe/Script/Ast/Types/AstTypeUShort.h>
#include <Lethe/Script/Ast/Types/AstTypeChar.h>
#include <Lethe/Script/Ast/Types/AstTypeEnum.h>
#include <Lethe/Script/Ast/Types/AstTypeInt.h>
#include <Lethe/Script/Ast/Types/AstTypeUInt.h>
#include <Lethe/Script/Ast/Types/AstTypeLong.h>
#include <Lethe/Script/Ast/Types/AstTypeULong.h>
#include <Lethe/Script/Ast/Types/AstTypeFloat.h>
#include <Lethe/Script/Ast/Types/AstTypeDouble.h>
#include <Lethe/Script/Ast/Types/AstTypeName.h>
#include <Lethe/Script/Ast/Types/AstTypeString.h>
#include <Lethe/Script/Ast/Types/AstTypeAuto.h>
#include <Lethe/Script/Ast/Types/AstTypeArrayRef.h>
#include <Lethe/Script/Ast/Types/AstTypeDynamicArray.h>
#include <Lethe/Script/Ast/Types/AstTypeDelegate.h>
#include <Lethe/Script/Ast/Types/AstTypeStruct.h>
#include <Lethe/Script/Ast/Types/AstTypeClass.h>
#include <Lethe/Script/Ast/Types/AstTypeDef.h>

#include <Lethe/Script/Ast/AstImport.h>
