#include "AutoCompleteEngine.h"
#include "ScriptEngine.h"

#include "Ast/AstNode.h"
#include "Ast/AstText.h"
#include "Ast/Function/AstFunc.h"
#include "Ast/NamedScope.h"
#include "Ast/Types/AstTypeEnum.h"

#include <Lethe/Core/String/StringBuilder.h>

#include <Lethe/Core/Io/MemoryStream.h>
#include <Lethe/Core/Io/VfsFile.h>

#include <Lethe/Core/Collect/HashSet.h>
#include <Lethe/Core/Collect/Queue.h>
#include <Lethe/Core/Ptr/UniquePtr.h>

#include <Lethe/Core/Lexer/Lexer.h>



namespace lethe
{

// AutoCompleteScope

AutoCompleteScope::~AutoCompleteScope()
{
}

// AutoCompleteEngine

AutoCompleteEngine::AutoCompleteEngine()
{
	engine = new ScriptEngine(ENGINE_RELEASE);
	openFile.Set(this, &AutoCompleteEngine::DefaultOpenFile);
}

AutoCompleteEngine::~AutoCompleteEngine()
{
}

void AutoCompleteEngine::PrettyPrintNode(Int argIndex, const AstNode *node, StringBuilder &sb, const String *memberName)
{
	if (!node)
		return;

	switch(node->type)
	{
	case AST_OP_SCOPE_RES:
		sb += AstStaticCast<const AstText *>(node)->text;
		break;

	case AST_IDENT:
		sb += AstStaticCast<const AstText *>(node)->text;
		break;

	case AST_NPROP_METHOD:
		if (memberName)
		{
			sb += *memberName;

			if (node)
			{
				auto text = AstStaticCast<const AstText *>(node)->text;
				// try to find func
				auto *nprop = node->scopeRef->FindSymbol(text, false, true);

				if (nprop && nprop->type == AST_FUNC)
				{
					sb += "(";
					PrettyPrintNode(argIndex, nprop->nodes[AstFunc::IDX_ARGS], sb);
					sb += ")";
					break;
				}
			}
			sb += "(?)";
		}
		break;

	case AST_FUNC:
		PrettyPrintNode(-1, node->nodes[AstFunc::IDX_NAME], sb);
		sb += "(";
		PrettyPrintNode(argIndex, node->nodes[AstFunc::IDX_ARGS], sb);
		sb += ")";
		break;

	case AST_ARG_ELLIPSIS:
		sb += "...";
		break;

	case AST_ARG_LIST:
		for (Int i=0; i<node->nodes.GetSize(); i++)
		{
			if (i == argIndex)
				sb += "#ff0000";
			else
				sb += "#808040";

			PrettyPrintNode(-1, node->nodes[i], sb);

			sb += "#000000";

			if (i+1 < node->nodes.GetSize())
				sb += ", ";
		}
		break;

	case AST_ARG:
		// [0] = type, [1] = name, [2] = (optional) default init
		PrettyPrintNode(-1, node->nodes[1], sb);
		break;
	default:;
	}
}

AutoCompleteLocation AutoCompleteEngine::FindDefinition(Int col, Int line, const String &nfilename) const
{
	AutoCompleteLocation res;

	ReadMutexLock lock(mutex);

	auto *rn = engine->GetFindDefinitionRoot();

	if (!rn)
		return res;

	auto *def = rn->FindDefinition(col, line, nfilename);

	if (!def)
		return res;

	res.column = def->location.column;
	res.line = def->location.line;
	res.file = def->location.file;

	return res;
}

Stream *AutoCompleteEngine::DefaultOpenFile(const String &nfilename) const
{
	UniquePtr<Stream> res = new VfsFile(nfilename);

	return res->IsOpen() ? res.Detach() : nullptr;
}

void AutoCompleteEngine::AddFile(const String &nfilename)
{
	WriteMutexLock lock(mutex);

	FileInfo fi;
	fi.filename = nfilename;

	files[nfilename] = fi;
}

void AutoCompleteEngine::AddFileWithContents(const String &nfilename, const String &ncontents)
{
	WriteMutexLock lock(mutex);

	FileInfo fi;
	fi.filename = nfilename;
	fi.contents = ncontents;
	fi.contentsValid = true;

	files[nfilename] = fi;
}

bool AutoCompleteEngine::CloseFile(const String &nfilename)
{
	WriteMutexLock lock(mutex);

	auto it = files.Find(nfilename);

	if (it == files.End())
		return false;

	files.Erase(it);

	return true;
}

bool AutoCompleteEngine::RenameFile(const String &oldfilename, const String &newfilename)
{
	WriteMutexLock lock(mutex);

	auto it = files.Find(oldfilename);

	if (it == files.End())
		return false;

	auto inf = it->value;
	inf.filename = newfilename;
	files.Erase(it);
	files[newfilename] = inf;

	return true;
}

bool AutoCompleteEngine::SetFileContents(const String &nfilename, const String &ncontents)
{
	WriteMutexLock lock(mutex);

	auto it = files.Find(nfilename);

	if (it == files.End())
		return false;

	it->value.contents = ncontents;
	it->value.contentsValid = true;

	return true;
}

// clear file contents, use open instead
bool AutoCompleteEngine::ClearFileContents(const String &nfilename)
{
	WriteMutexLock lock(mutex);

	auto it = files.Find(nfilename);

	if (it == files.End())
		return false;

	it->value.contents.Clear();
	it->value.contentsValid = false;

	return true;
}

void AutoCompleteEngine::AddForcedImport(const String &nfilename)
{
	WriteMutexLock lock(mutex);

	forcedImports.Add(nfilename);
}

void AutoCompleteEngine::RunTasksInThread()
{
}

void AutoCompleteEngine::TryUpdate(const String &nfilename)
{
	// we need to check for pending updates, if pending, do nothing
	(void)nfilename;
}

bool AutoCompleteEngine::FullUpdate()
{
	WriteMutexLock lock(mutex);

	SharedPtr<ScriptEngine> tempEngine = new ScriptEngine(ENGINE_RELEASE);

#if 0
	tempEngine->onError = [](const String &msg, const TokenLocation &loc)
	{
		LogLevel(LOG_ERROR, "%s at %d in %s", msg.Ansi(), loc.line, loc.file.Ansi());
	};
#endif

	for (auto &&it : forcedImports)
	{
		UniquePtr<Stream> str = OpenFileInternal(it);

		if (!str || !tempEngine->CompileStream(*str, it))
			return false;
	}

	// here we sort all files
	Array<String> sorted(files.GetSize());

	for (Int i=0; i<sorted.GetSize(); i++)
		sorted[i] = files.GetKey(i).key;

	sorted.Sort();

	for (auto &&it : sorted)
	{
		UniquePtr<Stream> str = OpenFileInternal(it);

		if (!str || !tempEngine->CompileStream(*str, it))
			return false;
	}

	if (!tempEngine->Link(LINK_KEEP_COMPILER | LINK_SKIP_CODEGEN))
		return false;

	tempEngine.SwapWith(engine);
	return true;
}

Stream *AutoCompleteEngine::OpenFileInternal(const String &nfilename) const
{
	auto it = files.Find(nfilename);

	if (it != files.End() && it->value.contentsValid)
		return new MemoryStream(it->value.contents.Ansi(), it->value.contents.GetLength());

	return openFile(nfilename);
}

AutoCompleteScope AutoCompleteEngine::FindScopeInternal(Int col, Int line, const String &nfilename) const
{
	(void)col;

	AutoCompleteScope res;

	auto *rn = engine->GetFindDefinitionRoot();

	if (!rn)
		return res;

	AstConstIterator iter(rn);

	Int bestLine = -1;

	while (auto *n = iter.Next())
	{
		if (!n->scopeRef || n->location.line > line || n->location.file != nfilename)
			continue;

		if (n->location.line > bestLine)
		{
			bestLine = n->location.line;
			res.engineRef = engine;
			res.nodeRef = n;
			res.scopeRef = n->scopeRef;
		}
	}

	return res;
}

AutoCompleteScope AutoCompleteEngine::FindScope(Int col, Int line, const String &nfilename) const
{
	ReadMutexLock lock(mutex);
	return FindScopeInternal(col, line, nfilename);
}

Array<AutoCompleteHint> AutoCompleteEngine::GetHints(Int col, Int line, const String &nfilename, const StringRef &ncontext) const
{
	// we need to get best hints from context, we don't know how far back we have to go
	Array<AutoCompleteHint> res;

	if (ncontext.IsEmpty())
		return res;

	ReadMutexLock lock(mutex);

	auto scope = FindScopeInternal(col, line, nfilename);

	if (!scope)
		return res;

	StringBuilder sb;

	// remove useless stuff first
	for (auto ch : ncontext)
	{
		switch(ch.ch)
		{
		case ';':
		case '{':
		case '}':
		case '=':
			// reset on special characters
			sb.Clear();
			continue;
		}

		sb += ch.ch;
	}

	// now we need to parse sb, best guess

	if (sb.Get().IsEmpty())
		return res;

	Lexer lex(LEXM_LETHE);
	MemoryStream ms(sb.Get().Ansi(), sb.Get().GetLength());

	if (!lex.Open(ms, String()))
		return res;

	Token tok;

	const NamedScope *nscope = scope.scopeRef;

	HashSet<String> processed;

	auto resolveTarget = [](const AstNode *node)->AstNode *
	{
		if (!node)
			return nullptr;

		// FIXME: this is quite stupid
		if (node->type == AST_ARG)
			node = node->nodes[0];

		auto *targ = node->GetResolveTarget();

		while (targ && targ->type == AST_TYPEDEF)
			targ = targ->GetResolveTarget();

		if (targ && targ->type == AST_TYPE_AUTO)
			targ = targ->target;

		return targ;
	};

	auto addHint = [&](const String &mname, const AstNode *nnode, Int argIndex)
	{
		if (processed.FindIndex(mname) >= 0)
			return;

		processed.Add(mname);

		AutoCompleteHint ahint;
		ahint.name = mname;

		switch(nnode ? nnode->type : AST_NONE)
		{
		case AST_FUNC:
		case AST_NPROP_METHOD:
			ahint.hint = AutoCompleteHint::HINT_FUNCTION;
			break;

		case AST_ARG:
			ahint.hint = AutoCompleteHint::HINT_ARGUMENT;
			break;

		case AST_VAR_DECL:
		case AST_NPROP:
			ahint.hint = AutoCompleteHint::HINT_VARIABLE;

			if (nnode->type == AST_VAR_DECL)
			{
				auto qualifiers = nnode->parent->nodes[0]->qualifiers;

				if (qualifiers & (AST_Q_CONST | AST_Q_CONSTEXPR))
					ahint.hint = AutoCompleteHint::HINT_CONSTANT;
			}
			break;

		case AST_ENUM_ITEM:
		case AST_ENUM:
			ahint.hint = AutoCompleteHint::HINT_CONSTANT;
			break;

		case AST_STRUCT:
		case AST_CLASS:
		case AST_TYPEDEF:
			ahint.hint = AutoCompleteHint::HINT_TYPE;
			break;

		default:;
		}

		if (nnode && nnode->IsConstant())
			ahint.hint = AutoCompleteHint::HINT_CONSTANT;

		StringBuilder sb;

		PrettyPrintNode(argIndex, nnode, sb, &mname);

		ahint.text = sb.Get();

		if (ahint.text.IsEmpty())
			ahint.text = ahint.name;

		ahint.argIndex = argIndex;

		res.Add(ahint);
	};

	String lastIdent;

	auto clearLastIdent = [&]()
	{
		lastIdent.Clear();
	};

	// TODO: array indexers and more!

	bool noparent = false;

	TokenType lastToken = TOK_INVALID;

	Int nesting = 0;
	Int argIndex = -1;

	bool isFunction = false;

	for (;;)
	{
		auto tt = lex.GetToken(tok);

		StringRef sr(tok.text);

		if (tt == TOK_INVALID || tt == TOK_EOF)
			break;

		if (nesting > 0)
		{
			switch(tt)
			{
			case TOK_LARR:
			case TOK_LBR:
				++nesting;
				break;

			case TOK_COMMA:
				argIndex += nesting == 1;
				break;

			case TOK_RARR:
			case TOK_RBR:

				if (!--nesting)
				{
					if (isFunction)
					{
						nscope = nullptr;
						clearLastIdent();
					}
				}
				break;
			default:;
			}

			continue;
		}

		auto olastToken = lastToken;
		lastToken = tt;

		if (tt == TOK_LARR)
		{
			// here we need to resolve!

			if (nscope)
				if (auto *tmp = nscope->FindSymbol(lastIdent, true, true))
					if (auto *targ = resolveTarget(tmp))
					{
						// must be an array
						// in theory, this could as well be an indexable struct or struct with index operator - we don't deal with these (yet)
						switch(targ->type)
						{
						case AST_TYPE_ARRAY:
						case AST_TYPE_ARRAY_REF:
						case AST_TYPE_DYNAMIC_ARRAY:
							targ = resolveTarget(targ->nodes[0]);

							nscope = targ ? targ->scopeRef : nullptr;
							break;

						default:
							nscope = nullptr;
						}

						clearLastIdent();
						continue;
					}

			clearLastIdent();
			++nesting;
			continue;
		}

		if (tt == TOK_LBR)
		{
			// do something special
			if (nscope)
				if (auto *tmp = nscope->FindSymbol(lastIdent, true, true))
					if (tmp->type == AST_FUNC || tmp->type == AST_NPROP_METHOD)
					{
						++nesting;
						argIndex = 0;
						isFunction = true;
						continue;
					}

			clearLastIdent();
			continue;
		}

		if (tt == TOK_KEY_THIS)
		{
			clearLastIdent();

			if (nscope)
				nscope = nscope->FindThis(true);

			continue;
		}

		if (tt == TOK_DOUBLE_COLON)
		{
			// still need to handle global scope
			if (olastToken != TOK_INVALID)
				noparent = true;

			if (lastIdent == "super")
			{
				if (nscope)
					nscope = nscope->FindThis(true);

				if (nscope)
					nscope = nscope->base;

				clearLastIdent();
				continue;
			}

			if (lastIdent.IsEmpty())
			{
				// go all the way up to root scope
				while (nscope && nscope->parent)
					nscope = nscope->parent;

				continue;
			}

			// find named scope
			if (nscope)
			{
				const NamedScope *nsout;
				auto *tmp = nscope->FindSymbolFull(lastIdent, nsout);

				if (tmp && tmp->type == AST_TYPEDEF)
					tmp = resolveTarget(tmp);

				if (tmp && tmp->type == AST_ENUM)
				{
					// enums are a bit tricky...
					// it's [0] = enum name, [1] = enum type, [2]+ = items
					if (tmp->nodes.GetSize() > AstTypeEnum::IDX_FIRST_ITEM)
						tmp = tmp->nodes[AstTypeEnum::IDX_FIRST_ITEM];
				}

				if (tmp)
					nscope = tmp->scopeRef;
				else
					nscope = nullptr;

				clearLastIdent();
			}
		}

		if (tt == TOK_DOT)
		{
			noparent = true;

			// okay we need to go through this!
			if (nscope)
				if (auto *tmp = nscope->FindSymbol(lastIdent, true, true))
					if (auto *targ = resolveTarget(tmp))
					{
						nscope = targ->scopeRef;
						clearLastIdent();
						continue;
					}

			nscope = nullptr;
			clearLastIdent();

			continue;
		}

		if (tt == TOK_IDENT)
		{
			lastIdent = tok.text;
			continue;
		}
	}

	// finalizing scope
	Queue<const NamedScope *> squeue;

	bool showHints = false;

	switch (lastToken)
	{
	case TOK_DOT:
	case TOK_DOUBLE_COLON:
	case TOK_LBR:
		showHints = true;
		break;
	case TOK_IDENT:
		showHints = true;
		break;
	default:;
	}

	if (showHints && nscope)
		squeue.PushBack(nscope);

	// up to 50
	while (!squeue.IsEmpty() && res.GetSize() < 50)
	{
		auto *tmp = squeue.Front();
		squeue.PopFront();

		if (noparent && tmp->type == NSCOPE_GLOBAL)
			continue;

		if (!noparent && tmp->parent)
			squeue.PushBack(tmp->parent);

		if (tmp->base)
			squeue.PushBack(tmp->base);

		for (auto &&it : tmp->members)
			if (isFunction ? it.key == lastIdent : lastIdent.IsEmpty() || it.key.StartsWith(lastIdent))
				addHint(it.key, it.value, argIndex);
	}

	return res;
}


}
