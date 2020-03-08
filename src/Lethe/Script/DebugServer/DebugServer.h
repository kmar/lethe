#pragma once

#include "../Common.h"

#include <Lethe/Core/Ptr/UniquePtr.h>
#include <Lethe/Core/Ptr/RefCounted.h>
#include <Lethe/Core/Thread/Thread.h>
#include <Lethe/Script/Vm/Vm.h>
#include <Lethe/Core/Delegate/Delegate.h>

#include "Socket.h"

namespace lethe
{

class ScriptEngine;
class ScriptContext;
struct TokenLocation;

class LETHE_API DebugServer : public RefCounted
{
public:
	explicit DebugServer(
		ScriptEngine &nengine,
		const String &ndebuggerIP,
		Int ndebuggerPort
	);
	~DebugServer();

	bool Start();

	ScriptEngine &GetEngine() const {return *engine;}

	// send to debugger
	void SendOutput(const String &msg);

	UInt GetReloadCounter() const;

	// hot reload callbacks; call these if reload succeeded/failed
	void OnReloadSuccessful();
	void OnReloadFailed();

	bool OnDebugBreak(ScriptContext &ctx, ExecResult &);

	// this callback is used to load the contents of a specific script file
	// input: filename, output: contents in String
	Delegate<String(const String &)> onReadScriptFile;

	// this callback will be used to send script filename list to the debugger
	Delegate<Array<String>()> onGetScriptFilenames;

private:
	String debuggerIP;
	Int debuggerPort;

	enum DebugStepCommand
	{
		DSTEP_NONE,
		DSTEP_INTO,
		DSTEP_OVER,
		DSTEP_OUT
	};

	ScriptEngine *engine;

	Mutex clientMutex;
	UniquePtr<Thread> serverThread;
	Array<Socket *> clients;
	Array<UniquePtr<Thread>> clientThreads;

	UniquePtr<Socket> serverThreadSocket;

	AtomicUInt reloadCounter = 0;

	void StartDebugServer();
	void ServerThreadProc();
	void ClientThreadProc(Thread *nthread, Socket *nsocket);

	void DebugSendToClients(const StringRef &msg);

	void OnDebugError(const String &msg, const TokenLocation &loc, Int warnId);
};

}
