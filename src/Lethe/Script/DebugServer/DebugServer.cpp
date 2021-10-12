#include "DebugServer.h"
#include "Socket.h"
#include "NetModule.h"
#include "../ScriptEngine.h"

#include <Lethe/Core/Sys/Fs.h>
#include <Lethe/Core/Time/Timer.h>

namespace lethe
{

// DebugServer

DebugServer::DebugServer(
	ScriptEngine &nengine,
	const String &ndebuggerIP,
	Int ndebuggerPort
)
	: engine(&nengine)
	, debuggerIP(ndebuggerIP)
	, debuggerPort(ndebuggerPort)
{
	NetModule::Init();
}

DebugServer::~DebugServer()
{
	// force to quit server
	if (serverThreadSocket)
		serverThreadSocket->Close();

	{
		MutexLock lock(clientMutex);

		// and clients
		for (auto *it : clients)
			it->Close();
	}

	clientThreads.Clear();
	serverThread.Clear();

	NetModule::Done();
}

bool DebugServer::OnDebugBreak(ScriptContext &ctx, ExecResult &eres)
{
	// assuming connected...
	auto depth = ctx.GetCallStackDepth();

	auto &debugData = ctx.GetDebugData();

	if (eres == EXEC_EXCEPTION)
		debugData.activeStepCmd = DSTEP_NONE;

	if (depth <= 0)
	{
		ctx.Resume();
		Atomic::Store(debugData.stepCmd, (Int)DSTEP_NONE);
		debugData.activeStepCmd = DSTEP_NONE;
		return false;
	}

	{
		// this won't work as it only matches exact func start
		TokenLocation tloc;
		ctx.GetCurrentLocation(tloc);

		if (debugData.activeStepCmd == DSTEP_OVER)
		{
			auto fname = GetEngine().FindFunctionNameNear(ctx.GetStack().GetInsPtr());

			bool doBreak = false;

			if (fname == debugData.origFunction && tloc.file == debugData.origLoc.file && depth && depth >= debugData.activeCallStackDepth)
			{
				// if function is the same, break if location is different
				doBreak = tloc.line != debugData.origLoc.line;
			}

			if (!doBreak && depth && depth >= debugData.activeCallStackDepth &&
				(depth > debugData.activeCallStackDepth || tloc.file != debugData.origLoc.file || tloc.line <= debugData.origLoc.line))
				return false;

			debugData.activeStepCmd = DSTEP_NONE;
		}

		if (debugData.activeStepCmd == DSTEP_INTO)
		{
			if (tloc.line == debugData.origLoc.line)
				return false;

			const auto &ctol = ctx.GetStack().GetProgram().codeToLine;

			if (!ctol.IsEmpty())
			{
				auto *iptr = ctx.GetStack().GetInsPtr();
				auto *ibeg = ctx.GetStack().GetProgram().instructions.GetData();

				auto pc = iptr - ibeg;

				// ignore internal generated ctor/dtor/copy
				if (pc < ctol[0].pc)
					return false;
			}

			debugData.activeStepCmd = DSTEP_NONE;
		}

		if (debugData.activeStepCmd == DSTEP_OUT)
		{
			if (debugData.activeCallStackDepth && depth >= debugData.activeCallStackDepth)
				return false;

			debugData.activeStepCmd = DSTEP_NONE;
		}

		MutexLock lock(clientMutex);

		for (auto &it : clients)
		{
			// send debug_break command: context id, location/line, location/file
			it->SendData(String::Printf("debug_break\n%s\n%d\n%s", ctx.GetName().Ansi(), tloc.line, tloc.file.Ansi()));
		}
	}

	while (ctx.InBreakMode())
	{
		auto scmd = Atomic::Load(debugData.stepCmd);

		if (scmd)
		{
			ctx.GetCurrentLocation(debugData.origLoc);
			debugData.activeStepCmd = debugData.stepCmd;
			debugData.activeCallStackDepth = ctx.GetCallStackDepth();
			Atomic::Store(debugData.stepCmd, (Int)DSTEP_NONE);
			debugData.origFunction = GetEngine().FindFunctionNameNear(ctx.GetStack().GetInsPtr());
			break;
		}

		Thread::Sleep(1);
	}

	// continue execution

	MutexLock lock(clientMutex);

	// send continue exec
	for (auto &it : clients)
		it->SendData(String::Printf("debug_continue\n%s", ctx.GetName().Ansi()));

	return false;
}

bool DebugServer::Start()
{
	LETHE_RET_FALSE(!serverThread);
	StartDebugServer();
	return true;
}

bool DebugServer::WaitForDebugger(Int msec) const
{
	StopWatch sw;
	sw.Start();

	do
	{
		{
			MutexLock lock(clientMutex);

			if (!clients.IsEmpty())
				return true;
		}

		Thread::Sleep(10);
	} while (sw.Get() < msec);

	return false;
}

void DebugServer::StartDebugServer()
{
	// now: try to listen to incoming connection from the debugger
	serverThread = new Thread;
	serverThread->onWork = [this](){ServerThreadProc();};
	serverThread->Run();
}

void DebugServer::ClientThreadProc(Thread *nthread, Socket *nsocket)
{
	UniquePtr<Socket> usocket = nsocket;

	while (!nthread->GetKillFlag())
	{
		String buf;

		if (!nsocket->RecvData(buf))
			break;

		if (buf.StartsWith("query_file\n"))
		{
			auto cmds = buf.Split("\n");

			if (cmds.GetSize() < 2)
				continue;

			auto contents = onReadScriptFile(cmds[1]);
			DebugSendToClients(String::Printf("file\n%s\n%s", cmds[1].Ansi(), contents.Ansi()));

			continue;
		}

		if (buf == "get_project_folder")
		{
			// now this is problematic... it could be config path OR CWD...
			// I think I fix cwd if necessary on Mac so it should work (hopefully on Linux too)
			DebugSendToClients(String::Printf("project_folder\n%s", Fs::GetCwd().Ansi()));
			continue;
		}

		if (buf.StartsWith("goto_definition\n"))
		{
			auto cmds = buf.Split("\n");

			if (cmds.GetSize() < 4)
				continue;

			cmds.EraseIndex(0);

			// [0] = file, [1] = col, [2] = line

			auto *root = GetEngine().GetFindDefinitionRoot();

			if (!root)
				continue;

			auto *def = root->FindDefinition(cmds[1].AsInt(), cmds[2].AsInt(), cmds[0]);

			if (!def)
				continue;

			DebugSendToClients(String::Printf("goto_definition\n%s\n%d\n%d", def->location.file.Ansi(), def->location.line, def->location.column));
			continue;
		}

		if (buf == "reload_scripts")
		{
			Atomic::Increment(reloadCounter);

			// first, resume all contexts and delete all breakpoints
			GetEngine().DeleteAllBreakpoints();
			auto ctxs = GetEngine().GetContexts();

			for (auto &it : ctxs)
				it->Resume();

			continue;
		}

		if (buf == "getcontexts")
		{
			auto ctxs = GetEngine().GetContexts();

			StringBuilder sb;
			sb = "getcontexts\n";

			for (auto &&it : ctxs)
				sb.AppendFormat("%s\n", it->GetName().Ansi());

			nsocket->SendData(sb.Get());
			continue;
		}

		if (buf == "getinternalscript")
		{
			StringBuilder sb;
			sb = "internalscript\n";
			sb += GetEngine().GetInternalProgram();

			nsocket->SendData(sb.Get());
			continue;
		}

		if (buf == "disassemble")
		{
			auto sz = GetEngine().GetByteCodeSize();

			StringBuilder sb;
			sb = "disassembly\n";

			for (Int i=0; i<sz; i++)
			{
				sb.AppendFormat("%s\n", GetEngine().DisassembleByteCode(i).Ansi());
			}

			nsocket->SendData(sb.Get());
			continue;
		}

		if (buf.StartsWith("toggle_breakpoint\n"))
		{
			auto cmds = buf.Split("\n");

			if (cmds.GetSize() < 2)
				continue;

			auto line = cmds[2].AsInt();
			auto pc = GetEngine().GetBreakpointPCList(cmds[1], line);

			for (auto &&it : pc)
				GetEngine().ToggleBreakpoint(cmds[1], it);

			continue;
		}

		if (buf.StartsWith("continue_context\n"))
		{
			auto cmds = buf.Split("\n");

			if (cmds.GetSize() < 2)
				continue;

			auto ctx = GetEngine().GetContexts();

			for (auto &it : ctx)
			{
				if (it->GetName() == cmds[1])
				{
					it->Resume();
					break;
				}
			}

			continue;
		}

		if (buf.StartsWith("getcallstack\n"))
		{
			auto cmds = buf.Split("\n");

			if (cmds.GetSize() < 2)
				continue;

			auto ctxs = GetEngine().GetContexts();

			StringBuilder sb;
			sb = "getcallstack\n";

			for (auto &&it : ctxs)
			{
				if (it->GetName() != cmds[1])
					continue;

				if (!it->InBreakMode())
					break;

				auto cstk = it->GetCallStack(65536);

				for (auto &&it2 : cstk)
					sb.AppendFormat("%s\n", it2.Ansi());
			}

			nsocket->SendData(sb.Get());
			continue;
		}

		if (buf == "break_all")
		{
			auto ctxs = GetEngine().GetContexts();

			for (auto &it : ctxs)
				it->Break();

			continue;
		}

		if (buf == "delete_all_breakpoints")
		{
			GetEngine().DeleteAllBreakpoints();
			continue;
		}

		if (buf == "resume_all")
		{
			auto ctxs = GetEngine().GetContexts();

			for (auto &it : ctxs)
				it->Resume();

			continue;
		}

		if (buf == "getfiles")
		{
			// send back list of script file paths
			StringBuilder sb;

			auto scripts = onGetScriptFilenames();

			// we want to be deterministic
			scripts.Sort();

			sb.AppendFormat("getfiles\n");

			for (auto &&it : scripts)
				sb.AppendFormat("%s\n", it.Ansi());

			nsocket->SendData(sb.Get());
			continue;
		}

		if (buf.StartsWith("step_over\n") || buf.StartsWith("step_into\n") || buf.StartsWith("step_out\n"))
		{
			auto cmds = buf.Split("\n");

			if (cmds.GetSize() < 2)
				continue;

			auto ctxs = GetEngine().GetContexts();

			for (auto &it : ctxs)
			{
				if (it->GetName() == cmds[1])
				{
					auto &debugData = it->GetDebugData();

					if (cmds[0] == "step_over")
						Atomic::Store(debugData.stepCmd, (Int)DSTEP_OVER);
					else if (cmds[0] == "step_into")
						Atomic::Store(debugData.stepCmd, (Int)DSTEP_INTO);
					else if (cmds[0] == "step_out")
						Atomic::Store(debugData.stepCmd, (Int)DSTEP_OUT);
					break;
				}
			}

			continue;
		}
	}

	MutexLock lock(clientMutex);
	auto idx = clients.FindIndex(usocket.Get());
	LETHE_ASSERT(idx >= 0);
	clients.EraseIndex(idx);
}

void DebugServer::ServerThreadProc()
{
	auto *thisThread = serverThread.Get();

	serverThreadSocket = new Socket(true);
	serverThreadSocket->Bind(debuggerIP, debuggerPort);

	GetEngine().onInfo("listening for debugger...");

	if (!serverThreadSocket->Listen())
	{
		GetEngine().onInfo("listen failed on debugger socket");
		return;
	}

	while (!thisThread->GetKillFlag())
	{
		auto *connected = serverThreadSocket->Accept();

		if (connected)
		{
			GetEngine().onInfo("debugger connected");
			MutexLock lock(clientMutex);
			clients.Add(connected);

			// clean up completed client threads
			for (Int i=0; i<clientThreads.GetSize(); i++)
				if (clientThreads[i]->HasCompleted())
					clientThreads.EraseIndexFast(i--);

			auto *cthread = new Thread;

			clientThreads.Add(cthread);

			cthread->onWork = [this, cthread, connected]()
			{
				ClientThreadProc(cthread, connected);
			};
			cthread->Run();
		}

		Thread::Sleep(50);
	}
}

void DebugServer::OnReloadSuccessful()
{
	DebugSendToClients("reload_success");
}

void DebugServer::OnReloadFailed()
{
	DebugSendToClients("reload_failure");
}

void DebugServer::DebugSendToClients(const StringRef &msg)
{
	MutexLock lock(clientMutex);

	for (auto &it : clients)
		it->SendData(msg);
}

void DebugServer::OnDebugError(const String &msg, const TokenLocation &loc, Int warnId)
{
	DebugSendToClients(String::Printf("error\n%s\n%d\n%d\n%s\n%d", loc.file.Ansi(), loc.line, loc.column, msg.Ansi(), warnId));
}

void DebugServer::SendOutput(const String &msg)
{
	DebugSendToClients(String::Printf("debug_output\n%s\n", msg.Ansi()));
}

UInt DebugServer::GetReloadCounter() const
{
	return Atomic::Load(reloadCounter);
}

}
