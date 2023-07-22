#pragma once

#include "../Common.h"

#include <Lethe/Core/Io/StreamDecl.h>
#include <Lethe/Core/Io/TokenStream.h>
#include <Lethe/Core/Delegate/Delegate.h>
#include <Lethe/Core/Ptr/UniquePtr.h>
#include <Lethe/Core/Ptr/RefCounted.h>
#include <Lethe/Core/Ptr/SharedPtr.h>
#include <Lethe/Core/String/Name.h>
#include <Lethe/Core/Collect/HashSet.h>
#include <Lethe/Core/Collect/HashMap.h>
#include <Lethe/Core/Thread/Atomic.h>
#include <Lethe/Core/Lexer/Lexer.h>
#include <Lethe/Core/Io/Stream.h>
#include <Lethe/Script/Ast/AstNode.h>
#include <Lethe/Script/Ast/AstText.h>
#include <Lethe/Script/Ast/NamedScope.h>
#include <Lethe/Script/TypeInfo/Attributes.h>

#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

class AstText;

class LETHE_API Compiler
{
public:
	struct Threaded {};

	// error/warning delegate
	Delegate< void(const String &msg, const TokenLocation &loc) > onError;
	Delegate< void(const String &msg, const TokenLocation &loc, Int warnid) > onWarning;
	// compile start delegate
	Delegate< void(const String &fname) > onCompile;
	// resolve steps callbacks
	Delegate< void(Int steps) > onResolve;

	Compiler();
	Compiler(Threaded, AtomicInt *pstaticInitCtr);
	~Compiler();

	void SetFloatLiteralIsDouble(bool nfloatLitIsDouble);

	// made public for testing
	bool Open(Stream &s, const String &nfilename);

	// necessary if this object is to be reused
	void Clear();

	// note: this is pass 0, no semantic checks
	// uses local buffering, consuming less memory, but cannot measure IO overhead
	AstNode *Compile(Stream &s, const String &nfilename);

	// special version which reads full files into internal buffer
	// can be used to measure IO time
	AstNode *CompileBuffered(Stream &s, const String &nfilename, Double *ioTime = nullptr );

	bool AddCompiledProgram(AstNode *node);

	// merge with another compiled data
	bool Merge(Compiler &c);

	// final resolve
	bool Resolve(bool ignoreErrors = false);

	// code gen
	bool CodeGen(CompiledProgram &p);

	// public_debug:
	AstNode *ParseExpression(Int depth = 0);
	const AstNode *GetRootNode() const
	{
		return progList.Get();
	}

	AtomicInt &GetStaticInitCounter() const
	{
		return staticInitCounter;
	}

private:
	static const Int MAX_DEPTH = 1024;

	struct AccessGuard
	{
		AccessGuard(Compiler *ncomp, ULong nqualifiers)
			: oldStructAccess(ncomp->structAccess)
			, comp(ncomp)
		{
			comp->structAccess = nqualifiers;
		}

		~AccessGuard()
		{
			comp->structAccess = oldStructAccess;
		}

	private:
		ULong oldStructAccess;
		Compiler *comp;
	};

	struct NamedScopeGuard
	{
		NamedScopeGuard()
			: comp(nullptr)
			, oldScope(nullptr)
		{
		}

		NamedScopeGuard(Compiler *ncomp, NamedScope *newCur)
		{
			Init(ncomp, newCur);
		}

		void Init(Compiler *ncomp, NamedScope *newCur)
		{
			comp = ncomp;
			oldScope = ncomp->currentScope;
			LETHE_ASSERT(!newCur->parent || newCur->parent == oldScope || newCur == oldScope);
			ncomp->currentScope = newCur;
		}

		~NamedScopeGuard()
		{
			if (comp)
				comp->currentScope = oldScope;
		}

	private:
		Compiler *comp;
		NamedScope *oldScope;
	};

	TokenMacroMap macroMap;

	HashSet< String > stringTable;
	UniquePtr<TokenStream> ts;
	UniquePtr<Lexer> lex;
	UniquePtr<AstNode> progList;

	UniquePtr<NamedScope> globalScope;
	NamedScope *currentScope;
	AstNode *currentProgram;
	// implied struct/class access (private/protected/none=public)
	ULong structAccess;

	// special scopes for resolving native types
	NamedScope *nullScope;
	NamedScope *stringScope;
	NamedScope *arrayScope;
	NamedScope *arrayRefScope;
	NamedScope *dynamicArrayScope;
	UniquePtr<AstNode> nullNode;
	Array<UniquePtr<AstNode>> nativeNodes;

	// if >0, error message won't be shown
	Int nofail;
	// counter to allow to parse closing dynamic arrays with >> or >>> tokens
	Int templateAccum;
	// static init counter for __init and __exit global functions
	mutable AtomicInt staticInitCounter;
	AtomicInt *pstaticInitCounter;

	// imported files
	HashSet<String> imported;
	// files to import
	Array<String> import;

	// only used by CompileBuffered
	Array<Byte> fileBuffer;
	SharedPtr<Stream> tempStream;

	// current attributes (should be consumed by next type/var decl)
	SharedPtr<Attributes> attributes;

	// temporary instance, used by macro expression evaluation (for constant folding)
	UniquePtr<CompiledProgram> tempProgram;

	bool floatLitIsDouble;
	Int classOpen = 0;

	bool OpenBuffered(Stream &s, const String &nfilename, Double *ioTime);
	AstNode *CompileBufferedInternal(Stream &s, const String &nfilename, Double *ioTime);

	// string table
	const String &AddString(const char *str);
	const String &AddStringRef(StringRef sr);

	bool ExpectLoc(bool expr, const char *msg, const TokenLocation &nloc);
	bool Expect(bool expr, const char *msg);
	// previous token; i.e. we've already moved past it
	bool ExpectPrev(bool expr, const char *msg);

	bool CheckDepth(Int ndepth);

	bool AddScopeMember(const String &nname, AstNode *nnode, bool isCtor = false);
	bool AddScopeLabel(const String &nname, AstNode *nnode);

	NamedScope *AddUniqueNamedScope(const String &nname);
	NamedScope *FindAddNamedScope(const String &nname);

	AstNode *CreateConstNumber(const Token &t);

	AstNode *ParseCommaExpression(Int depth);
	AstNode *ParseAssignExpression(Int depth);
	AstNode *ParseLOrExpression(Int depth);
	AstNode *ParseLAndExpression(Int depth);
	AstNode *ParseOrExpression(Int depth);
	AstNode *ParseXorExpression(Int depth);
	AstNode *ParseAndExpression(Int depth);
	AstNode *ParseEqExpression(Int depth);
	AstNode *ParseLtGtExpression(Int depth);
	AstNode *ParseShiftExpression(Int depth);
	AstNode *ParseAddExpression(Int depth);
	AstNode *ParseMultExpression(Int depth);
	AstNode *ParseName(Int depth);
	AstNode *ParseStructName(Int depth);
	AstNode *ParseScopeResolution(Int depth);
	// TODO: refactor those two
	AstNode *ParsePriority2Operators(Int depth, UniquePtr<AstNode> &first);
	AstNode *ParsePriority2(Int depth, bool termOnly = 0);
	AstNode *ParseUnaryExpression(Int depth);

	// returns or mask
	ULong ParseQualifiers(bool ref = 0);

	// returns 1 if break/continue is legal
	bool IsValidBreak(bool isCont) const;

	AstNode *ParseTypeDef(Int depth);
	AstNode *ParseUsing(Int depth);

	bool ParseMacro(Int depth, bool conditionalOnly = false);
	bool ParseMacroInternal(bool conditionalOnly);
	bool ParseMacroArgs(Array<Token> &nargs);

	AstNode *ParseSwitchBody(Int depth);
	AstNode *ParseStatement(Int depth);
	AstNode *ParseNoBreakStatement(Int depth);
	AstNode *ParseBlock(Int depth, bool isFunc = false, bool noCheck = false, bool isStateFunc = false, const String *fname = nullptr);

	AstNode *ParseVarDeclOrExpr(Int depth, bool refFirstInit = 0, bool initOnly = 1);

	AstNode *ParseEnumDecl(UniquePtr<AstNode> &ntype, Int depth);
	AstNode *ParseStructDecl(UniquePtr<AstNode> &ntype, Int depth);
	AstNode *ParseClassDecl(UniquePtr<AstNode> &ntype, Int depth);

	AstNode *ParseFuncArgsDecl(Int depth);
	AstNode *ParseFuncDecl(UniquePtr<AstNode> &ntype,
						   UniquePtr<AstNode> &nname, Int depth);
	AstNode *ParseInitializerList(Int depth);
	AstNode *ParseVarDecl(UniquePtr<AstNode> &ntype,
						  UniquePtr<AstNode> &nname, Int depth, bool refFirstInit = 0,
						  bool initOnly = 0);
	AstNode *ParseFuncOrVarDecl(UniquePtr<AstNode> &ntype, Int depth);
	// returns refptr to type root (with qualifiers)
	AstNode *ParseArrayType(UniquePtr<AstNode> &ntype, Int depth);
	AstNode *ParseType(Int depth, bool init = true);
	AstNode *ParseTypeWithQualifiers(Int depth, ULong nqualifiers, bool init);
	AstNode *ParseSimpleType(Int depth, ULong nqualifiers);
	bool ParseSimpleTemplateType(AstNode *tmp, Int depth, bool init);
	AstNode *ParseQualifiedDecl(ULong qualifiers, Int depth);
	bool ParseDirective(Int lineNumber);
	Attributes *ParseAttributes();
	AstNode *ParseProgram(Int depth, const String &nfilename);

	AstNode *ParseStaticAssert(Int depth = 0);

	AstNode *ParseAnonStructLiteral(Int depth);

	AstNode *ParseShorthandFunction(Int depth, AstNode *ntype, const String &fname);

	bool ParseReturn(Int depth, UniquePtr<AstNode> &res);

	AstNode *NewAstType(TokenType tt, const TokenLocation &nloc) const;

	// echoes false
	bool Error(const String &msg);
	void Warning(const String &msg, Int level = 1);
	void ErrorLoc(const String &msg, const TokenLocation &nloc);
	void WarningLoc(const String &msg, const TokenLocation &nloc, Int level = 1);

	static void DefaultOnError(const String &msg, const TokenLocation &loc);
	static void DefaultOnWarning(const String &msg, const TokenLocation &loc, Int warnid);

	template< typename T >
	AstNode *NewAstNode(AstNodeType ntype, const TokenLocation &nloc) const;
	template< typename T >
	AstNode *NewAstNode(const TokenLocation &nloc) const;
	template< typename T >
	AstText *NewAstText(const char *ntext, AstNodeType ntype, const TokenLocation &nloc);
	template< typename T >
	AstText *NewAstText(const char *ntext, const TokenLocation &nloc);
	template< typename T >
	AstText *NewAstTextRef(StringRef ntext, const TokenLocation &nloc);

	bool ValidateVirtualProp(const AstNode *pfun, bool isGetter);

	// initialize resolve scopes for native types/arrays
	void InitNativeTypeScopes();

	AstNode *AddNativeProp(const char *nname, AstNodeType type);

	// move external functions (bodies) into struct/class scope
	bool MoveExternalFunctions(ErrorHandler &eh);

	// template support: instantiate templates
	bool InstantiateTemplates(ErrorHandler &eh);

	static bool GenerateTemplateName(ErrorHandler &eh, const String &qname, StringBuilder &sb, AstNode *instanceNode, bool &nestedTemplate);

	// inject array/string scopes
	void InjectScopes(ErrorHandler &eh);

	// conditional compilation:

	bool ConditionalEnabled() const;

	bool EnterIfMacro(bool cond, Int depth, bool nopush);
	bool EndIfMacro();

	// conditional stack flags
	enum
	{
		CSF_ACTIVE = 1,
		CSF_IF_TAKEN = 2,
		CSF_GOT_ELSE = 4
	};

	StackArray<Int, 32> conditionalStack;
	Int conditionalSkipCounter = 0;

	// allow C emulation via * and ->
	bool allowCEmulation = true;

	void InitTokenStream();
};

template< typename T >
AstNode *Compiler::NewAstNode(AstNodeType ntype, const TokenLocation &nloc) const
{
	AstNode *res = new T(ntype, nloc);
	res->scopeRef = currentScope;
	return res;
}

template< typename T >
AstNode *Compiler::NewAstNode(const TokenLocation &nloc) const
{
	AstNode *res = new T(nloc);
	res->scopeRef = currentScope;
	return res;
}

template< typename T >
AstText *Compiler::NewAstText(const char *ntext, AstNodeType ntype, const TokenLocation &nloc)
{
	AstText *res = new T(AddString(ntext), ntype, nloc);
	res->scopeRef = currentScope;
	return res;
}

template< typename T >
AstText *Compiler::NewAstText(const char *ntext, const TokenLocation &nloc)
{
	AstText *res = new T(AddString(ntext), nloc);
	res->scopeRef = currentScope;
	return res;
}

template< typename T >
AstText *Compiler::NewAstTextRef(StringRef ntext, const TokenLocation &nloc)
{
	AstText *res = new T(AddStringRef(ntext), nloc);
	res->scopeRef = currentScope;
	return res;
}

}
