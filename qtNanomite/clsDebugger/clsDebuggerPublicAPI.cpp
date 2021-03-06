#include "clsDebugger.h"
#include "clsMemManager.h"
#include "clsDBManager.h"
#include "clsAPIImport.h"

#include "dbghelp.h"

using namespace std;

bool clsDebugger::DetachFromProcess()
{
	_NormalDebugging = true;
	_isDebugging = false;
	_bStopDebugging = true;

	RemoveBPs();

	for(size_t d = 0;d < PIDs.size();d++)
	{
		if(!CheckProcessState(PIDs[d].dwPID))
			break;
		DebugBreakProcess(PIDs[d].hProc);
		DebugActiveProcessStop(PIDs[d].dwPID);
		PulseEvent(_hDbgEvent);
	}

	emit OnDebuggerTerminated();
	return true;
}

bool clsDebugger::AttachToProcess(DWORD dwPID)
{
	CleanWorkSpace();
	_NormalDebugging = false;_dwPidToAttach = dwPID;
	return true;
}

bool clsDebugger::SuspendDebuggingAll()
{
	for(size_t i = 0;i < PIDs.size();i++)
		SuspendDebugging(PIDs[i].dwPID);
	CleanWorkSpace();
	return true;
}

bool clsDebugger::SuspendDebugging(DWORD dwPID)
{
	if(CheckProcessState(dwPID))
	{
		if(dbgSettings.dwSuspendType == 0x0)
		{
			HANDLE hProcess = NULL;
			for(size_t i = 0;i < PIDs.size();i++)
			{
				if(PIDs[i].bRunning && PIDs[i].dwPID == dwPID)
					hProcess = PIDs[i].hProc;
			}

			if(DebugBreakProcess(hProcess))
			{
				memset(tcLogString,0x00,LOGBUFFER);
				swprintf_s(tcLogString,LOGBUFFERCHAR,L"[!] %X Debugging suspended!",dwPID);
				PBLogInfo();
				return true;
			}
		}
		else// if(dbgSettings.dwSuspendType == 0x1)
		{
			if(SuspendProcess(dwPID,true))
			{
				memset(tcLogString,0x00,LOGBUFFER);
				swprintf_s(tcLogString,LOGBUFFERCHAR,L"[!] %X Debugging suspended!",dwPID);
				PBLogInfo();
				return true;
			}
		}
	}
	return false;
}

bool clsDebugger::StopDebuggingAll()
{
	for(size_t i = 0;i < PIDs.size();i++)
		StopDebugging(PIDs[i].dwPID);
	return PulseEvent(_hDbgEvent);
}

bool clsDebugger::StopDebugging(DWORD dwPID)
{
	HANDLE hProcess = GetCurrentProcessHandle(dwPID);

	if(CheckProcessState(dwPID))
	{
		if(TerminateProcess(hProcess,0))
		{
			return true;
		}
	}
	return false;
}

bool clsDebugger::ResumeDebugging()
{
	for(size_t i = 0;i < PIDs.size(); i++)
		SuspendProcess(PIDs[i].dwPID,false);
	return PulseEvent(_hDbgEvent);
}

bool clsDebugger::RestartDebugging()
{
	StopDebuggingAll();
	Sleep(2000);
	StartDebugging();
	return true;
}

bool clsDebugger::GetDebuggingState()
{
	if(_isDebugging == true)
		return true;
	else
		return false;
}

bool clsDebugger::IsTargetSet()
{
	if(_sTarget.length() > 0)
		return true;
	return false;
}

bool clsDebugger::StepOver(quint64 dwNewOffset)
{
	for (vector<BPStruct>::iterator it = SoftwareBPs.begin(); it != SoftwareBPs.end();++it) {
		if(it->dwHandle == 0x2)
		{
			dSoftwareBP(it->dwPID,it->dwOffset,it->dwSize,it->bOrgByte);
			SoftwareBPs.erase(it);
			it = SoftwareBPs.begin();
		}
		if(SoftwareBPs.size() <= 0)
			break;
	}

	BPStruct newBP;
	newBP.dwOffset = dwNewOffset;
	newBP.dwHandle = 0x2;
	newBP.dwSize = 0x1;
	newBP.bOrgByte = NULL;
	newBP.dwPID = _dwCurPID;

	wSoftwareBP(newBP.dwPID,newBP.dwOffset,newBP.dwHandle,newBP.dwSize,newBP.bOrgByte);

	if(newBP.bOrgByte != 0xCC)
	{
		SoftwareBPs.push_back(newBP);
		PulseEvent(_hDbgEvent);
		return true;
	}
	return false;
}

bool clsDebugger::StepIn()
{
	_bSingleStepFlag = true;
	ProcessContext.EFlags |= 0x100;
	return PulseEvent(_hDbgEvent);
}

bool clsDebugger::ShowCallStack()
{

	if(_dwCurTID == 0 || _dwCurPID == 0)
		return false;

	HANDLE hProc,hThread = OpenThread(THREAD_GETSET_CONTEXT,false,_dwCurTID);
	PSYMBOL_INFOW pSymbol = (PSYMBOL_INFOW)malloc(sizeof(SYMBOL_INFOW) + MAX_SYM_NAME);
	DWORD dwMaschineMode = NULL;
	quint64 dwDisplacement;
	LPVOID pContext;
	IMAGEHLP_MODULEW64 imgMod = {0};
	STACKFRAME64 stackFr = {0};

	int iPid = 0;
	for(size_t i = 0;i < PIDs.size(); i++)
	{
		if(PIDs[i].dwPID == _dwCurPID)
			hProc = PIDs[i].hProc;iPid = i;
	}

	if(!PIDs[iPid].bSymLoad)
		PIDs[iPid].bSymLoad = SymInitialize(hProc,NULL,false);

#ifdef _AMD64_
	BOOL bIsWOW64 = false;
	HANDLE hProcess = NULL;

	if(clsAPIImport::pIsWow64Process)
		clsAPIImport::pIsWow64Process(_hCurProc,&bIsWOW64);

	if(bIsWOW64)
	{
		dwMaschineMode = IMAGE_FILE_MACHINE_I386;
		WOW64_CONTEXT wowContext;
		pContext = &wowContext;
		wowContext.ContextFlags = WOW64_CONTEXT_ALL;
		clsAPIImport::pWow64GetThreadContext(hThread,&wowContext);

		stackFr.AddrPC.Mode = AddrModeFlat;
		stackFr.AddrFrame.Mode = AddrModeFlat;
		stackFr.AddrStack.Mode = AddrModeFlat;
		stackFr.AddrPC.Offset = wowContext.Eip;
		stackFr.AddrFrame.Offset = wowContext.Ebp;
		stackFr.AddrStack.Offset = wowContext.Esp;
	}
	else
	{
		dwMaschineMode = IMAGE_FILE_MACHINE_AMD64;
		CONTEXT context;
		pContext = &context;
		context.ContextFlags = CONTEXT_ALL;
		GetThreadContext(hThread, &context);

		stackFr.AddrPC.Mode = AddrModeFlat;
		stackFr.AddrFrame.Mode = AddrModeFlat;
		stackFr.AddrStack.Mode = AddrModeFlat;
		stackFr.AddrPC.Offset = context.Rip;
		stackFr.AddrFrame.Offset = context.Rbp;
		stackFr.AddrStack.Offset = context.Rsp;	
	}
#else
	dwMaschineMode = IMAGE_FILE_MACHINE_I386;
	CONTEXT context;
	pContext = &context;
	context.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hThread, &context);

	stackFr.AddrPC.Mode = AddrModeFlat;
	stackFr.AddrFrame.Mode = AddrModeFlat;
	stackFr.AddrStack.Mode = AddrModeFlat;
	stackFr.AddrPC.Offset = context.Eip;
	stackFr.AddrFrame.Offset = context.Ebp;
	stackFr.AddrStack.Offset = context.Esp;
#endif

	BOOL bSuccess;
	do
	{
		bSuccess = StackWalk64(dwMaschineMode,hProc,hThread,&stackFr,pContext,NULL,SymFunctionTableAccess64,SymGetModuleBase64,0);

		if(!bSuccess)        
			break;

		memset(&imgMod,0x00,sizeof(imgMod));
		imgMod.SizeOfStruct = sizeof(imgMod);
		bSuccess = SymGetModuleInfoW64(hProc,stackFr.AddrPC.Offset, &imgMod);

		memset(pSymbol,0,sizeof(SYMBOL_INFOW) + MAX_SYM_NAME);
		pSymbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
		pSymbol->MaxNameLen = MAX_SYM_NAME;

		quint64 dwStackAddr = stackFr.AddrStack.Offset;

		quint64 dwEIP = stackFr.AddrPC.Offset;
		bSuccess = SymFromAddrW(hProc,dwEIP,&dwDisplacement,pSymbol);
		wstring sFuncName = pSymbol->Name;
		bSuccess = SymGetModuleInfoW64(hProc,dwEIP, &imgMod);
		wstring sFuncMod = imgMod.ModuleName;

		quint64 dwReturnTo = stackFr.AddrReturn.Offset;
		bSuccess = SymFromAddrW(hProc,dwReturnTo,&dwDisplacement,pSymbol);
		wstring sReturnToFunc = pSymbol->Name;
		bSuccess = SymGetModuleInfoW64(hProc,dwReturnTo,&imgMod);

		wstring sReturnToMod = imgMod.ModuleName;

		IMAGEHLP_LINEW64 imgSource;
		imgSource.SizeOfStruct = sizeof(imgSource);
		bSuccess = SymGetLineFromAddrW64(hProc,stackFr.AddrPC.Offset,(PDWORD)&dwDisplacement,&imgSource);

		if(bSuccess)
			emit OnCallStack(dwStackAddr,
			dwReturnTo,sReturnToFunc,sReturnToMod,
			dwEIP,sFuncName,sFuncMod,
			imgSource.FileName,imgSource.LineNumber);
		else
			emit OnCallStack(dwStackAddr,
			dwReturnTo,sReturnToFunc,sReturnToMod,
			dwEIP,sFuncName,sFuncMod,
			L"",0);

	}while(stackFr.AddrReturn.Offset != 0);

	free(pSymbol);
	CloseHandle(hThread);
	return false;
}

bool clsDebugger::ReadMemoryFromDebugee(DWORD dwPID,quint64 dwAddress,DWORD dwSize,LPVOID lpBuffer)
{
	for(size_t i = 0;i < PIDs.size();i++)
		if(PIDs[i].dwPID == dwPID)
			return ReadProcessMemory(PIDs[i].hProc,(LPVOID)dwAddress,lpBuffer,dwSize,NULL);
	return false;
}

bool clsDebugger::WriteMemoryFromDebugee(DWORD dwPID,quint64 dwAddress,DWORD dwSize,LPVOID lpBuffer)
{
	for(size_t i = 0;i < PIDs.size();i++)
		if(PIDs[i].dwPID == dwPID)
			return WriteProcessMemory(PIDs[i].hProc,(LPVOID)dwAddress,lpBuffer,dwSize,NULL);
	return false;
}

void clsDebugger::ClearTarget()
{
	_sTarget.clear();
}

void clsDebugger::SetTarget(wstring sTarget)
{
	_sTarget = sTarget;
	_NormalDebugging = true;
}

DWORD clsDebugger::GetCurrentPID()
{
	return _dwCurPID;
}

DWORD clsDebugger::GetCurrentTID()
{
	return _dwCurTID;
}

void clsDebugger::SetCommandLine(std::wstring CommandLine)
{
	_sCommandLine = CommandLine;
}

void clsDebugger::ClearCommandLine()
{
	_sCommandLine.clear();
}

HANDLE clsDebugger::GetCurrentProcessHandle()
{
	return _hCurProc;
}

wstring clsDebugger::GetCMDLine()
{
	return _sCommandLine;
}

wstring clsDebugger::GetTarget()
{
	return _sTarget;
}