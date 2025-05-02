#pragma once

#include "Common.h"

#include <Lethe/Core/Delegate/Delegate.h>
#include <Lethe/Core/Sys/NoCopy.h>
#include <Lethe/Core/Ptr/RefCounted.h>
#include <Lethe/Core/Ptr/SharedPtr.h>
#include <Lethe/Core/Ptr/UniquePtr.h>
#include <Lethe/Core/String/String.h>
#include <Lethe/Core/Io/StreamDecl.h>
#include <Lethe/Core/Thread/Lock.h>

#include <Lethe/Core/Collect/HashMap.h>
#include <Lethe/Core/Lexer/Token.h>

namespace lethe
{
class StringBuilder;
}

namespace lethe
{

class ScriptEngine;
class AstNode;
class NamedScope;
class ErrorHandler;

struct AutoCompleteLocation : public TokenLocation
{
	AutoCompleteLocation() {column = line = -1;}

	bool IsValid() const {return line >= 0;}
	bool operator !() const {return !IsValid();}
};

LETHE_API_BEGIN

struct LETHE_API AutoCompleteScope
{
	~AutoCompleteScope();

	bool operator !() const {return !scopeRef;}

	SharedPtr<ScriptEngine> engineRef;
	const NamedScope *scopeRef = nullptr;
	const AstNode *nodeRef = nullptr;
};

struct AutoCompleteHint
{
	enum HintType
	{
		HINT_UNKNOWN,
		HINT_FUNCTION,
		HINT_ARGUMENT,
		HINT_CONSTANT,
		HINT_VARIABLE,
		// a type in general
		HINT_TYPE,
		HINT_NAMESPACE
	};

	HintType hint = HINT_UNKNOWN;
	// plain name
	String name;
	// full text
	String text;
	// argument index if function
	Int argIndex = -1;
};

class LETHE_API AutoCompleteEngine : public NoCopy, public RefCounted
{
public:
	AutoCompleteEngine();
	virtual ~AutoCompleteEngine();

	// file management
	void AddFile(const String &nfilename);
	void AddFileWithContents(const String &nfilename, const String &ncontents);
	bool CloseFile(const String &nfilename);
	bool RenameFile(const String &oldfilename, const String &newfilename);

	// set file contents; this way we can override openFile
	bool SetFileContents(const String &nfilename, const String &ncontents);
	// clear file contents, use open instead
	bool ClearFileContents(const String &nfilename);

	// forced import go before everything else
	void AddForcedImport(const String &nfilename);

	// use this from a separate thread to do background processing
	void RunTasksInThread();

	// run a full update now
	// thread-safe but blocks
	bool FullUpdate();

	// try to update a file
	// should be called whenever the user types a ; or } in a file (of after undo/redo, copy/paste, line insertion or deletion)
	void TryUpdate(const String &nfilename);

	// find definition
	AutoCompleteLocation FindDefinition(Int col, Int line, const String &nfilename) const;

	// get autocompletion hints
	Array<AutoCompleteHint> GetHints(Int col, Int line, const String &nfilename, const StringRef &ncontext) const;

	AutoCompleteScope FindScope(Int col, Int line, const String &nfilename) const;

	// delegates
	// delegate to open a file; must be thread-safe
	Delegate<Stream *(const String &)> openFile;

private:
	mutable RWMutex mutex;

	struct FileInfo
	{
		String filename;
		String contents;
		bool contentsValid = false;
	};

	Array<String> forcedImports;

	HashMap<String, FileInfo> files;

	SharedPtr<ScriptEngine> engine;

	UniquePtr<ErrorHandler> eh;

	Stream *OpenFileInternal(const String &nfilename) const;

	Stream *DefaultOpenFile(const String &nfilename) const;

	static void PrettyPrintNode(Int argIndex, const AstNode *node, StringBuilder &sb, const String *memberName = nullptr);

	AutoCompleteScope FindScopeInternal(Int col, Int line, const String &nfilename) const;

	const NamedScope *GetTargetScope(const AstNode *nnode) const;
};

LETHE_API_END

}
