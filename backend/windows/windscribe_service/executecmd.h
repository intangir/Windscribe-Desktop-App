#ifndef EXECUTECMD_H
#define EXECUTECMD_H

#include "ipc/servicecommunication.h"
#include "pipe_for_process.h"

class ExecuteCmd
{
public:
    static ExecuteCmd &instance()
    {
        static ExecuteCmd i;
        return i;
    }

	MessagePacketResult executeBlockingCmd(wchar_t *cmd);
	MessagePacketResult executeUnblockingCmd(const wchar_t *cmd, const wchar_t *szEventName, const wchar_t *szWorkingDir);
    MessagePacketResult getUnblockingCmdStatus(unsigned long cmdId);
    MessagePacketResult getActiveUnblockingCmdCount();
	MessagePacketResult clearUnblockingCmd(unsigned long id);
    MessagePacketResult terminateAndClearUnblockingCmd(unsigned long id);

private:
    ExecuteCmd();
    virtual ~ExecuteCmd();

private:
    static unsigned long blockingCmdId_;

    struct BlockingCmd
    {
        unsigned long id;
		std::wstring szEventName;

		PipeForProcess pipeForProcess;
        HANDLE hProcess;
        HANDLE hThread;
        HANDLE hWait;

        bool bFinished;
        DWORD  dwExitCode;
        std::string strLogOutput;

        BlockingCmd() : id(0), hProcess(0), hThread(0), hWait(0), bFinished(false), dwExitCode(0)
        {
        }
    };

    std::vector<BlockingCmd *> blockingCmds_;
    std::mutex mutex_;

    static VOID CALLBACK waitOrTimerCallback(PVOID lpParameter, BOOLEAN timerOrWaitFired);
    static ExecuteCmd *this_;

    void terminateCmd(unsigned long id, unsigned long waitTimeout);
	void clearCmd(unsigned long id);
	void clearAllCmds();
};

#endif // EXECUTECMD_H
