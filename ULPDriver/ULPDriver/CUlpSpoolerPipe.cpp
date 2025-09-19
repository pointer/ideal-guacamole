#include <UserEnv.h>
#include "ulpHelperUsingLog.h"
#include "CUlpLog.h"
#include "ulpCharBuffer.h"
#include "CUlpSpoolerPipe.h"


/*-------------------------------------------------------
Spooler Interface
-------------------------------------------------------*/

void CUlpSpoolerPipe::CreatePipename(DWORD printingApplicationsProcessId)
{
    time_t timestamp; 
    time(&timestamp);
        
    m_PipeName = new ulpHelper::CharBuffer(100);
    DWORD dwProcessId;
    if (printingApplicationsProcessId != 0)
    {
        dwProcessId = printingApplicationsProcessId;
    }
    else
    {
        dwProcessId = GetProcessIdOfThread(GetCurrentThread());
    }

    _stprintf_s(m_PipeName->Buffer(), m_PipeName->Size(), _T("%s_%ld_%lld"), PipeNamePrefix, dwProcessId, timestamp);

    _Log->LogVar("PipeName", m_PipeName->GetBufferAnsi());
}

// Initialization of the spooler process 
void CUlpSpoolerPipe::InitAndStartSpooler(long _lDriverJobId)
{
    int level = _Log->EnterSection("InitSpooler");
    try 
    {
        m_SpoolerExeFullname = ulpHelper::ReadLogoPrintRegStrLog(HKEY_LOCAL_MACHINE, _T("LPSpoolerPath"), _Log);
        if (m_SpoolerExeFullname == NULL)
        {
            m_SpoolerIsDisabledDueToAnError = true;
        }
        else
        {
            m_ThreadId = GetCurrentThreadId();
        
            ZeroMemory(&m_SpoolerProcessInfo, sizeof(m_SpoolerProcessInfo));

            StartSpooler(_lDriverJobId);
        }
    }
    catch (const std::exception& e)
    {
        m_SpoolerIsDisabledDueToAnError = true;
        _Log->LogError("Error in InitSpooler", e);
    }
    _Log->ExitSection(level);

}


//Cleans resources used to start an communicate with spooler
void CUlpSpoolerPipe::CleanSpoolerResources()
{    
    int level = _Log->EnterSection("CleanSpoolerResources");
    try 
    {
        CleanResources();
    }
    catch (const std::exception& e)
    {
        _Log->LogError("Error in CleanSpoolerResources", e);
    }
    _Log->ExitSection(level);
}



/*-------------------------------------------------------
Cleaning spooler related resources
-------------------------------------------------------*/

//Closes spooler's ProcessInfo-handles
void CUlpSpoolerPipe::CleanSpoolerProcessInfo()
{
    _Log->LogLine("Cleaning spooler process data ... ");
    if (m_SpoolerProcessInfo.hProcess)
    {
        CloseHandle(m_SpoolerProcessInfo.hProcess);
    }
    if (m_SpoolerProcessInfo.hThread)
    {
        CloseHandle(m_SpoolerProcessInfo.hThread);
    }
    ZeroMemory(&m_SpoolerProcessInfo, sizeof(m_SpoolerProcessInfo));
    _Log->LogLine("Process data cleaned!");
}

//Closes the pipe-handle, frees buffers, deletes CiticalSection-handles, terminates the co-nnect-thread and frees messageLoop-resources
void CUlpSpoolerPipe::CleanResources()
{
    int level = _Log->EnterSection("Clean Resources");
    try 
    {
        if (m_PipeHandle) 
        {
            _Log->LogLine("Closing pipe ...");
            FlushFileBuffers(m_PipeHandle);
            CloseHandle(m_PipeHandle);
            m_PipeHandle = NULL;
            _Log->LogLine("Pipe closed!!");
        }
        else 
        {
            _Log->LogLine("Pipe already closed!");
        }


        if (m_SpoolerExeFullname) 
        { 
            _Log->LogLine("Freeing buffers ...");
            m_PipeName->free();
            m_PipeName = NULL;
            m_SpoolerExeFullname->free();
            m_SpoolerExeFullname = NULL;
            _Log->LogLine("Buffers freed!");
        }
        else 
        {
            _Log->LogLine("Buffers already freed!");
        }
    }
    catch (const std::exception& e)
    {
        _Log->LogError("Error in CleanResources", e);
    }
    _Log->ExitSection(level);
}


/*-------------------------------------------------------
Starting Spooler Process
-------------------------------------------------------*/

bool CUlpSpoolerPipe::StartProcessAsCurrentUser(ulpHelper::CharBuffer* commandLine)
{
    // Get token of the current user
    HANDLE hThreadToken = NULL;
    HANDLE hTokenDup = NULL;
    BOOL rc = OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE | TOKEN_READ | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_ADJUST_PRIVILEGES, TRUE, &hThreadToken);

    DWORD lastError = 0;

    if (rc)
    {
        _Log->LogLine("Could open thread token!");
    }
    else
    {
        _Log->LogLine("Could not open thread token!");
        lastError = GetLastError();

    }
    SetLastError(0);


    rc = DuplicateTokenEx(hThreadToken, TOKEN_IMPERSONATE | TOKEN_READ | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_ADJUST_PRIVILEGES, NULL, SecurityImpersonation, TokenPrimary, &hTokenDup);
    if (rc)
    {
        _Log->LogLine("Could duplicate thread token!");
    }
    else
    {
        _Log->LogLine("Could not duplicate thread token!");
        lastError = GetLastError();
    }
    SetLastError(0);
    CloseHandle(hThreadToken);
    SetLastError(0);


    BOOL result = FALSE;
    STARTUPINFO startupInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    if (ulpHelper::GetShowConsoleWindows(_Log))
    {
        startupInfo.wShowWindow = SW_SHOWNORMAL;
    }
    else
    {
        startupInfo.wShowWindow = SW_HIDE;
    }
    DWORD dwCreationFlags = NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT;

    if (hTokenDup == NULL || !rc)
    {
        _Log->LogLine("Didn't get current user's token and will create process ");
        result = CreateProcess(NULL, commandLine->Buffer(), NULL, NULL, FALSE, dwCreationFlags, NULL, NULL, &startupInfo, &m_SpoolerProcessInfo) != FALSE;

    }
    else
    {
        _Log->LogLine("Got current user's token and will create process as user ...");

        void* lpEnvironment = NULL;

        // Get all necessary environment variables of logged in user
        // to pass them to the process
        BOOL resultEnv = CreateEnvironmentBlock(&lpEnvironment, hTokenDup, FALSE);
        if (resultEnv == 0)
        {
            lastError = GetLastError();
            SetLastError(0);
        }

        // Start the process on behalf of the current user 
        BOOL bInheritHandles = FALSE;
        result = CreateProcessAsUser(hTokenDup, NULL, commandLine->Buffer(), NULL,
                                        NULL, bInheritHandles, dwCreationFlags, lpEnvironment, NULL,
                                        &startupInfo, &m_SpoolerProcessInfo);

        if (!result)
        {
            lastError = GetLastError();
        }

        if (lastError != 0)
        {
            _Log->LogVarL("LastError", lastError);
        }

        SetLastError(0);

        DestroyEnvironmentBlock(lpEnvironment);
        CloseHandle(hTokenDup);
    }

    return result != FALSE;
}

bool CUlpSpoolerPipe::StartSpoolerProcessAsUser(ulpHelper::CharBuffer* cmdLine)
{
    bool spoolerProcessCreated = false;
    int level = _Log->EnterSection("Create Spooler Process");
    try
    {
        spoolerProcessCreated = StartProcessAsCurrentUser(cmdLine);
        if (spoolerProcessCreated)
        {
            _Log->LogLine("Spooler process has been started!");
            Connect();
        }
        else
        {
            _Log->LogLastErrorMessage("!!! Spooler process couldn't be started!", true, false);
        }
    }
    catch (const std::exception & e)
    {
        _Log->LogError("Error in CreateProcess", e);
    }
    _Log->ExitSection(level);
    return spoolerProcessCreated;
}

//Starts the spooler
void CUlpSpoolerPipe::StartSpooler(long lDriverJobId)
{
    int levelOuter = _Log->EnterSection("StartSpooler");
    ulpHelper::CharBuffer* arguments = NULL;
    ulpHelper::CharBuffer* cmdLine = NULL;
    try
    {
        CreatePipename(lDriverJobId);

        DWORD currentProcessId = GetProcessIdOfThread(GetCurrentThread());
        arguments = new ulpHelper::CharBuffer(0x8000UL);
        _stprintf_s(arguments->Buffer(), arguments->Size(), _T(" %s %d %d"), m_PipeName->Buffer(), currentProcessId, lDriverJobId); //Leading space is needed to get the first argument !

        CleanSpoolerProcessInfo();
        _Log->LogLine("Start spooler process ...");
        _Log->LogVar("Application", m_SpoolerExeFullname->GetBufferAnsi());
        _Log->LogVar("Arguments", arguments->GetBufferAnsi());

        cmdLine = new ulpHelper::CharBuffer(4096);
        _tcscat_s(cmdLine->Buffer(), cmdLine->Size(), _T("\""));
        _tcscat_s(cmdLine->Buffer(), cmdLine->Size(), m_SpoolerExeFullname->Buffer());
        _tcscat_s(cmdLine->Buffer(), cmdLine->Size(), _T("\" "));
        _tcscat_s(cmdLine->Buffer(), cmdLine->Size(), arguments->Buffer());
        _Log->LogVar("CmdLine", cmdLine->GetBufferAnsi());

        StartSpoolerProcessAsUser(cmdLine);
    }
    catch (const std::exception& e)
    {
        _Log->LogError("Error in StartSpooler", e);
    }
    _Log->ExitSection(levelOuter);

}

    

//Tries to connect to the spooler (connecting means to open the pipe created by the spooler process)
void CUlpSpoolerPipe::Connect()
{
    bool isConnected = false;
    int levelConnect = _Log->EnterSection("Connect()");
    bool accessDenied = false;
    try 
    {
        DWORD tryToConnectTimeInSec = ulpHelper::GetIntRegHKCUHKLM(REGVALUE_ConnectTimeout, ConnectTimeoutDefault, const_cast<char*>("ConnectTimeout"), _Log);
        _Log->LogLineParts(const_cast<char*>("Will try to connect ..."), NULL);

        clock_t ticksStart = clock();
        while (!isConnected && !accessDenied && ((DWORD)((clock() - ticksStart)/CLOCKS_PER_SEC) < tryToConnectTimeInSec))
        {
            //Try to open the spooler-created pipe
            isConnected = TryConnect(&accessDenied);
            if (accessDenied)
            {
                _Log->LogLine(const_cast<char*>("!!! Access to logoprint spooler denied!"));
            }
            else if (!isConnected)
            {
                Sleep(250);
            }
            else
            {
                ; //Connection established
            }
        }

    } 
    catch (const std::exception& e)
    {
        _Log->LogError("Error in Connect", e);
    }
    _Log->ExitSection(levelConnect);
}


//Tries to open the spooler created pipe. Returns true, if pipe is opened
bool CUlpSpoolerPipe::TryConnect(bool *accessDenied)
{
    bool isConnected = false;
    try
    {
        DWORD dwFlags = 0; //  FILE_FLAG_OVERLAPPED
        _Log->LogLineParts("Trying to open the pipe (", _Log->INSERTTIME, ") ...", NULL);
        m_PipeHandle = CreateFile(m_PipeName->Buffer(), GENERIC_WRITE, 0, NULL,
                                    CREATE_ALWAYS, dwFlags, NULL);

        //could not create handle - server probably not running
        if (m_PipeHandle != INVALID_HANDLE_VALUE)
        {
            _Log->LogLine("Pipe opened and connected!");
            isConnected = true;
        }
        else 
        {
            HRESULT hresult = GetLastError();
            if (hresult == 2)
            {
                _Log->LogLine("... Spooler process didn't create the pipe yet!");
            }
            else
            {
                _Log->LogLine("!!! Could not open pipe!");
                *accessDenied = (hresult == 5);
                _Log->LogLastErrorMessage("!!! WIN32-Error opening pipe", true, false);
            }
        }
    }
    catch (const std::exception& e)
    {
        _Log->LogError("Error in TryConnect", e);
    }
    return isConnected;
}



/*-------------------------------------------------------
Write to spooler's pipe
-------------------------------------------------------*/

//Writes the buffer to the pipe
DWORD CUlpSpoolerPipe::WriteToSpoolerPipe(const char* buffer, DWORD bytesToWrite, DWORD* _dwWritePipeLastError)
{
    LPOVERLAPPED notOverlapped = NULL;
    DWORD bytesWritten = 0;
    while (bytesToWrite > 0)
    {
        bool bSuccess = WriteFile(m_PipeHandle, buffer, bytesToWrite, &bytesWritten, notOverlapped) != FALSE;
        if (bytesWritten >= bytesToWrite)
        {
            bytesToWrite = 0;
        }
        else
        {
            bytesToWrite -= bytesWritten;
        }

        buffer += bytesWritten;
        (*_dwWritePipeLastError) = 0;
        if (!bSuccess)
        {
            DWORD lastError = GetLastError();
            if (lastError == 0)
            {
                lastError = ERROR_WRITE_FAULT;
                SetLastError(lastError);
            }
            (*_dwWritePipeLastError) = lastError;
            _Log->LogLastErrorMessage("!!! Error WriteToSpoolerPipe", true, true);
            break;
        }
    }
    return bytesWritten;
}

