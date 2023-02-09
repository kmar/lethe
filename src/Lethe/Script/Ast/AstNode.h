#pragma once

#include "../Common.h"

#include <Lethe/Core/Collect/Queue.h>
#include <Lethe/Core/Sys/NoCopy.h>
#include <Lethe/Core/Sys/Types.h>
#include <Lethe/Core/String/String.h>
#include <Lethe/Core/String/StringBuilder.h>
#include <Lethe/Core/Lexer/Token.h>
#include <Lethe/Core/Ptr/RefCounted.h>
#include <Lethe/Core/Io/StreamDecl.h>
#include <Lethe/Core/Memory/BucketAlloc.h>
#include <Lethe/Script/TypeInfo/DataTypes.h>

namespace lethe
{

class ErrorHandler;
class CompiledProgram;
class DataType;

// AstNode

enum AstFlag : UShort
{
	// referenced flag, to tag unreferenced vars
	AST_F_REFERENCED = 1 << 0,
	// resolved flag (=> all vars/scopes/bases resolved for this node)
	AST_F_RESOLVED	= 1 << 1,
	// subexpression flag
	AST_F_SUBEXPR	= 1 << 2,
	// skip codegen flag
	AST_F_SKIP_CGEN	= 1 << 3,
	// lock flag to prevent recursion (TypeGen)
	AST_F_LOCK		= 1 << 4,
	// mark NRVO vars
	AST_F_NRVO		= 1 << 5,
	// used to mark active defer nodes
	AST_F_DEFER		= 1 << 6,
	// special flags for dynamic array:
	// push elem type
	AST_F_PUSH_TYPE	= 1 << 7,
	// push elem type size
	AST_F_PUSH_TYPE_SIZE = 1 << 8,
	AST_F_ARG1_ELEM	= 1 << 9,
	AST_F_ARG2_ELEM = 1 << 10,
	AST_F_RES_ELEM = 1 << 11,
	AST_F_RES_SLICE = 1 << 12,

	AST_F_TEMPLATE_INSTANCE = 1 << 14,
	// type generated flag
	AST_F_TYPE_GEN  = 1 << 15
};

enum AstNodeType : Short
{
	AST_NONE,
	// note: order of const types does matter
	AST_CONST_BOOL,
	AST_CONST_CHAR,
	AST_CONST_INT,
	AST_CONST_UINT,
	AST_CONST_LONG,
	AST_CONST_ULONG,
	AST_CONST_FLOAT,
	AST_CONST_DOUBLE,
	AST_CONST_NULL,
	AST_CONST_NAME,
	AST_CONST_STRING,
	AST_THIS,
	AST_SUPER,
	// identifier
	AST_IDENT,
	// scope resolution
	AST_OP_SCOPE_RES,
	// dot operator
	AST_OP_DOT,
	// ast subscript (array)
	AST_OP_SUBSCRIPT,
	// ternary ? :
	AST_OP_TERNARY,
	AST_UOP_PLUS,
	AST_UOP_MINUS,
	AST_UOP_NOT,
	AST_UOP_LNOT,
	AST_UOP_PREINC,
	AST_UOP_PREDEC,
	AST_UOP_POSTINC,
	AST_UOP_POSTDEC,
	AST_UOP_REF,
	AST_OP_MUL,
	AST_OP_DIV,
	AST_OP_MOD,
	AST_OP_ADD,
	AST_OP_SUB,
	AST_OP_SHL,
	AST_OP_SHR,
	AST_OP_LT,
	AST_OP_LEQ,
	AST_OP_GT,
	AST_OP_GEQ,
	AST_OP_EQ,
	AST_OP_NEQ,
	AST_OP_AND,
	AST_OP_XOR,
	AST_OP_OR,
	AST_OP_LAND,
	AST_OP_LOR,
	AST_OP_THROW,		// not used
	AST_OP_ASSIGN,
	AST_OP_ADD_ASSIGN,
	AST_OP_SUB_ASSIGN,
	AST_OP_MUL_ASSIGN,
	AST_OP_DIV_ASSIGN,
	AST_OP_MOD_ASSIGN,
	AST_OP_SHL_ASSIGN,
	AST_OP_SHR_ASSIGN,
	AST_OP_AND_ASSIGN,
	AST_OP_XOR_ASSIGN,
	AST_OP_OR_ASSIGN,
	AST_OP_SWAP,
	AST_OP_SWAP_NULL,
	AST_OP_COMMA,		// not used
	AST_NEW,
	AST_CAST_OR_CALL,	// [0] = cast_type or f_name, [1] = expr or args
	AST_CAST,
	AST_SIZEOF,
	AST_OFFSETOF,
	AST_ALIGNOF,
	AST_TYPEID,
	AST_CALL,

	AST_TYPE_VOID,
	AST_TYPE_BOOL,
	AST_TYPE_BYTE,
	AST_TYPE_SBYTE,
	AST_TYPE_SHORT,
	AST_TYPE_USHORT,
	AST_TYPE_CHAR,
	AST_TYPE_INT,
	AST_TYPE_UINT,
	AST_TYPE_LONG,
	AST_TYPE_ULONG,
	AST_TYPE_FLOAT,
	AST_TYPE_DOUBLE,
	AST_TYPE_NAME,
	AST_TYPE_STRING,
	AST_TYPE_AUTO,
	// array type: [0] = base type, [1] = (optional) size expression, mandatory for static arrays
	AST_TYPE_ARRAY,
	AST_TYPE_DYNAMIC_ARRAY,
	// [0] = base type
	AST_TYPE_ARRAY_REF,

	// function declaration argument list
	AST_ARG_LIST,
	// func decl argument, up to 3 nodes: [0] = type, [1] = name, [2] = init (optional)
	AST_ARG,
	AST_ARG_ELLIPSIS,
	// func decl
	AST_FUNC,
	// block statement
	AST_BLOCK,
	// program container
	AST_PROGRAM,
	// var decl list [0] = type, [1+] = var_decls
	AST_VAR_DECL_LIST,
	// var decl: [0] = name, [1] = init (optional)
	AST_VAR_DECL,
	// statements
	AST_EXPR,
	AST_RETURN,
	AST_RETURN_VALUE,
	AST_BREAK,
	AST_CONTINUE,
	AST_IF,
	AST_WHILE,
	AST_DO,
	AST_FOR,
	AST_FOR_RANGE,
	AST_SWITCH,
	AST_SWITCH_BODY,
	AST_CASE,
	AST_CASE_DEFAULT,
	// empty statement
	AST_EMPTY,
	// class/struct/enum decl
	AST_CLASS,
	AST_STRUCT,
	AST_ENUM,
	AST_ENUM_ITEM,
	AST_TYPEDEF,
	// base for struct/class decl
	AST_BASE,
	// no base for struct/class decl
	AST_BASE_NONE,
	// program list (=root)
	AST_PROGRAM_LIST,
	// function body
	AST_FUNC_BODY,
	// initializer list
	AST_INITIALIZER_LIST,
	// func ptr or delegate
	AST_TYPE_FUNC_PTR,
	AST_TYPE_DELEGATE,
	// namespace
	AST_NAMESPACE,
	AST_IMPORT,
	AST_DEFER,
	AST_DEFAULT_INIT,
	AST_STRUCT_LITERAL,
	// these three are virtual nodes for injecting native properties
	AST_NPROP,
	AST_NPROP_METHOD,
	// label and goto
	AST_LABEL,
	AST_GOTO,
	// static assert
	AST_STATIC_ASSERT,
	// bitfield sizeof/offsetof; both return 0 for normal fields
	AST_BITSIZEOF,
	AST_BITOFFSETOF
};

template<typename T>
inline T AstStaticCast(AstNode *n)
{
	return static_cast<T>(n);
}

template<typename T>
inline T AstStaticCast(const AstNode *n)
{
	return static_cast<T>(n);
}

class LETHE_API AstIterator
{
public:
	AstIterator(AstNode *node);
	// ast node type to exclude
	AstNode *Next(Int exclude = -1);

	// override to iterate selected nodes
	virtual bool Accept(AstNode *node) const;
private:
	Queue<AstNode *> queue;
};

class LETHE_API AstConstIterator
{
public:
	AstConstIterator(const AstNode *node);
	const AstNode *Next();

	// override to iterate selected nodes
	virtual bool Accept(const AstNode *node) const;
private:
	Queue<const AstNode *> queue;
};

#define LETHE_AST_NODE(cls) \
	protected: cls() {} \
	public: AstNode *Clone() const override {auto *res = new cls;CopyTo(res);return res;}

class AstSymbol;

class LETHE_API AstNode : public NoCopy
{
	LETHE_BUCKET_ALLOC(AstNode)
public:
	AstNode(AstNodeType ntype, const TokenLocation &nloc);

	virtual ~AstNode();

	AstNode *parent;
	// resolved target (for symbols)
	AstNode *target;
	// node type
	AstNodeType type;
	// flags so that we save qualifiers
	UShort flags;
	// variable offset (relative to stack frame for functions)
	Int offset;

	typedef StackArray< AstNode *, 2> Nodes;
	Nodes nodes;

	mutable NamedScope *scopeRef;
	// symbol scope ref
	mutable const NamedScope *symScopeRef;

	mutable ULong qualifiers;

	union num
	{
		Int i;
		UInt ui;
		Long l;
		ULong ul;
		Float f;
		Double d;
	} num;
	TokenLocation location;
	// cached index at parent
	Int cachedIndex;

	enum ResolveResult
	{
		RES_OK,
		RES_MORE,
		RES_ERROR
	};

	// clone node (note that scopes are still intact after cloning)
	// must be overridden in each node with custom bucket allocator
	virtual AstNode *Clone() const;
	// copy contents to node
	// must be chained
	virtual void CopyTo(AstNode *n) const;

	// returns true on success
	// override in type nodes
	virtual bool GetTemplateTypeText(StringBuilder &sb) const;
	void AppendTypeQualifiers(StringBuilder &sb) const;

	// add new child node; echoes n
	AstNode *Add(AstNode *n);

	// unbind n-th node
	AstNode *UnbindNode(Int idx);
	// bind n-th node
	AstNode *BindNode(Int idx, AstNode *n);

	// delete child nodes
	void ClearNodes();

	// note: old not deleted
	bool ReplaceChild(AstNode *oldc, AstNode *newc);

	// is right-associative binary op? (actually ternary ?: counts as well)
	bool IsRightAssocBinaryOp() const;

	// override this
	// returns false on error
	virtual bool ResolveNode(const ErrorHandler &e);

	// by default a node is assumed to generate no code so it just iterates over children
	virtual bool CodeGen(CompiledProgram &p);

	bool CodeGenNoBreak(CompiledProgram &p);

	// used for initializer list codegen
	virtual bool GenInitializerList(CompiledProgram &p, QDataType qdt, Int ofs, bool global);
	// all elements initialized?
	virtual bool IsCompleteInitializerList(CompiledProgram &p, QDataType qdt) const;
	// holds only constants?
	virtual bool IsInitializerConst(const CompiledProgram &p, QDataType qdt) const;

	// special codegen for global ctors
	bool CodeGenGlobalCtor(CompiledProgram &p);

	// override this; special codegen for composite types (ctor/dtor/assignment)
	virtual bool CodeGenComposite(CompiledProgram &p);

	// can pass by reference?
	virtual bool CanPassByReference(const CompiledProgram &) const
	{
		return false;
	}

	// LValue codegen; generates code that pushes target ref on stack
	// problem is we want to optimize this of course if it's elem type on stack or global
	virtual bool CodeGenRef(CompiledProgram &p, bool allowConst = 0, bool derefPtr = 0);

	// override this;
	// by default a it just iterates over children
	// this is used for composite types; called after constant folding
	virtual bool TypeGen(CompiledProgram &p);

	// this is used for typedefs which are generated in first pass before TypeGen
	virtual bool TypeGenDef(CompiledProgram &p);

	// generate vtbls
	virtual bool VtblGen(CompiledProgram &p);

	// extra pre-pass for enum items
	virtual bool BeginCodegen(CompiledProgram &p);

	// fold constant expressions
	// returns true on change
	virtual bool FoldConst(const CompiledProgram &p);

	// returns RES_MORE if changed
	virtual ResolveResult Resolve(const ErrorHandler &e);
	ResolveResult ResolveFrom(const ErrorHandler &e, Int fromIdx);
	bool IsResolved() const;

	// is elementary type?
	virtual bool IsElemType() const;
	// promote small int types to int, used by ADL validation
	static AstNodeType PromoteSmallType(AstNodeType ntype);

	virtual AstNode *GetResolveTarget() const;

	// is constant expression?
	// only some AST nodes returns 0
	virtual bool IsConstExpr() const;

	// returns 1 if AST node is a constant (=literal)
	bool IsConstant() const;

	// used to optimize if/while/...
	// 0  = evaluates to false
	// 1  = evalutes to true
	// <0 => not a constant
	virtual Int ToBoolConstant(const CompiledProgram &p);

	// returns true if constant literal can be zero-inited
	virtual bool IsZeroConstant(const CompiledProgram &p) const;

	// try to dereference symbolic constant, null if none
	virtual AstNode *DerefConstant(const CompiledProgram &p);

	// returns null on error, otherwise potentially new node
	virtual AstNode *ConvertConstTo(DataTypeEnum dte, const CompiledProgram &p);
	AstNode *ConvertConstNode(const DataType &dt, DataTypeEnum dte, const CompiledProgram &p);

	virtual QDataType GetTypeDesc(const CompiledProgram &p) const;

	void Dump(Stream &s, Int level = 0) const;
	virtual String GetTextRepresentation() const;

	// get symbol scope
	const NamedScope *GetSymScope(bool parentOnly) const;

	// get type node for AstVarDecl/AstArg/...
	virtual const AstNode *GetTypeNode() const;

	// for resolving .ENUM_ITEM
	virtual const AstNode *GetContextTypeNode(const AstNode *node) const;

	// find symbol node + defining scope
	virtual AstNode *FindSymbolNode(String &sname, const NamedScope *&nscope) const;
	// find (leftmost) variable symbol
	// preferLocal is required for simple static analysis, namely for the ternary operator
	virtual AstSymbol *FindVarSymbolNode(bool preferLocal = false);

	inline bool IsUnaryOp() const
	{
		return type == AST_CAST || (type >= AST_UOP_PLUS && type <= AST_UOP_POSTDEC);
	}

	inline bool IsBinaryOp() const
	{
		return type != AST_OP_THROW && type >= AST_OP_MUL && type <= AST_OP_OR_ASSIGN;
	}

	const AstNode *FindDefinition(Int col, Int line, const String &filename) const;

	virtual void LoadIfVarDecl(CompiledProgram &p);

	// get tree depth
	Int GetDepth() const;

	// for assignments/increments and so on
	bool ShouldPop() const;

	// get template name we're in
	String FindTemplateName() const;

	virtual AstNode *ResolveTemplateScope(AstNode *&text) const;

	struct AdlResolveData
	{
		AstNode *node;
		Int depth;

		// reverse order; we want deepest first
		inline bool operator <(const AdlResolveData &o) const
		{
			return depth > o.depth;
		}
	};

	// this extracts ADL nodes (calls) in sorted order; deepest are first
	Array<AdlResolveData> GetAdlResolveNodes();

	// remove AST_Q_SKIP_DTOR in the case of AST_CALL or AST_NEW
	static void FixPointerQualifiers(QDataType &ntype, const AstNode *nnode);

protected:
	AstNode() {}

	// minimum unroll instruction weight
	static constexpr Int UNROLL_MIN_WEIGHT = 16;
	// maximum unroll count
	static constexpr Int UNROLL_MAX_COUNT = 8;

	virtual bool CodeGenGlobalCtorStatic(CompiledProgram &p);

	bool EmitPtrLoad(const QDataType &dt, CompiledProgram &p);

	bool BakeGlobalData(AstNode *n, QDataType qdt, Int ofs, CompiledProgram &p);

	bool ValidateMethod(CompiledProgram &p, const NamedScope *nscope, AstNode *nfunc) const;

	static DataType *GenFuncType(AstNode *fref, CompiledProgram &p, AstNode *resType, const Array<AstNode *> &args, bool isDelegate = false);

	void GetAdlResolveNodesInternal(Array<AdlResolveData> &resData, Int ndepth);

	static DataTypeEnum TypeEnumFromNode(const AstNode *n);
	static const AstNode *CoerceTypes(const AstNode *type0, const AstNode *type1);
};


}
