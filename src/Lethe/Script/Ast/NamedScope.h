#pragma once

#include "../Common.h"

#include <Lethe/Core/Ptr/RefCounted.h>
#include <Lethe/Core/Ptr/SharedPtr.h>
#include <Lethe/Core/Ptr/UniquePtr.h>
#include <Lethe/Core/String/String.h>
#include <Lethe/Core/Collect/HashMap.h>
#include <Lethe/Core/String/StringBuilder.h>

#include <Lethe/Script/TypeInfo/DataTypes.h>

namespace lethe
{

class Compiler;

enum NamedScopeType
{
	NSCOPE_NONE,
	// global scope
	NSCOPE_GLOBAL,
	// namespace scope
	NSCOPE_NAMESPACE,
	// func args scope
	NSCOPE_ARGS,
	// local block scope
	NSCOPE_LOCAL,
	// local loop scope (for break/continue)
	NSCOPE_LOOP,
	// local func scope
	NSCOPE_FUNCTION,
	// switch scope (for break)
	NSCOPE_SWITCH,
	NSCOPE_STRUCT,
	NSCOPE_CLASS
};

class AstNode;

class LETHE_API NamedScope : public RefCounted
{
	LETHE_BUCKET_ALLOC(NamedScope)
public:
	NamedScope();
	explicit NamedScope(NamedScopeType ntype);

	// parent scope refptr
	NamedScope *parent;
	// base for structs/classes
	NamedScope *base;
	// scope type
	NamedScopeType type;
	// can be empty
	String name;
	// to support templates
	String nameAlias;
	// corresponding AST node
	AstNode *node;
	// scope members
	HashMap< String, AstNode * > members;
	// scope operators (for structs)
	Array<AstNode *> operators;
	// sub-scopes (unnamed)
	Array< SharedPtr< NamedScope > > scopes;
	// named sub-scopes
	HashMap< String, SharedPtr<NamedScope> > namedScopes;
	// goto labels
	HashMap<String, AstNode *> labels;
	// for functions, index of check stack opcode (<0 = none)
	Int chkStkIndex;

	struct LocalVariable
	{
		Int offset;
		QDataType type;
	};

	Array< LocalVariable > localVars;

	NamedScope *Add(NamedScope *nsc);
	// full recursive scan
	AstNode *FindSymbolFull(const String &sname, const NamedScope *&nscope, bool baseOnly = false) const;
	// this one doesn't recurse by default
	AstNode *FindSymbol(const StringRef &sname, bool chainbase = false, bool chainparent = false) const;

	// full recursive scan until function scope
	AstNode *FindLabel(const String &sname) const;

	// no recursion
	AstNode *FindOperator(const CompiledProgram &p, const char *opName, const QDataType &ntype) const;
	AstNode *FindOperator(const CompiledProgram &p, const char *opName, const QDataType &ltype, const QDataType &rtype) const;

	// get fully qualified scope name
	void GetFullScopeName(StringBuilder &sb) const;

	const NamedScope *FindThis(bool allowStatic = false) const;
	const NamedScope *FindFunctionScope() const;

	bool IsBaseOf(const NamedScope *nscope) const;
	bool IsParentOf(const NamedScope *nscope) const;

	// within const method?
	bool IsConstMethod() const;

	// forward handles
	void AddBreakHandle(Int handle);
	void AddContinueHandle(Int handle);

	bool HasBreakHandles() const;

	bool FixupBreakHandles(CompiledProgram &p);
	bool FixupContinueHandles(CompiledProgram &p);

	// is local scope?
	bool IsLocal() const;
	// is global scope?
	bool IsGlobal() const;
	// is composite type scope?
	bool IsComposite() const;
	// not local and not global => struct/class scope

	// has destructors for local vars?
	bool HasDestructors() const;

	// generate dtors for local vars if necessary
	void GenDestructors(CompiledProgram &p, Int baseLocalVar = 0);

	bool SetBase(NamedScope *nbase);

	// merge with scope; used in multi-threaded parsing
	bool Merge(NamedScope &ns, const Compiler &c, HashMap<NamedScope *, NamedScope *> &scopeRemap);

	// allocate variable
	Int AllocVar(const QDataType &ndesc, bool alignStack = 1);
	Int varOfs;
	Int varSize;
	Int maxVarAlign;
	Int maxVarSize;

	AstNode *resultPtr;

	// for var decl after other statements
	bool needExtraScope;

	// flag to detect dup ctors
	bool ctorDefined;

	// helper for vardecl of statics
	Int blockThis;

	// to support nested scope exits during deferred codegen
	Int deferredTop;

	// deferred statements
	Array<AstNode *> deferred;

	// reset deferred top
	void ResetDeferredTop();

private:
	// break/continue handles
	Array< Int > breakHandles;
	Array< Int > continueHandles;
};


}
