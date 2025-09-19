//  Copyright  UniCredit S.p.A.
//
//  FILE:      ulpSpoolerPipe.h
//
//  PURPOSE:   Header for ULPSpooler related functions
//

#pragma once

#include <windows.h>
#include <process.h>
#include <tchar.h>
#include <cstdio>
#include <ctime>
#include "CUlpLog.h"
#include "ulpHelperUsingLog.h"


class CUlpSpoolerPipe
{

private:
    // Prefix used to build pipename for communication with ULPSPooler 
    const TCHAR* PipeNamePrefix = _T("\\\\.\\pipe\\UniLogoPrintSpooler");
    const LPCTSTR REGVALUE_ConnectTimeout = _T("ConnectTimeout");

    ulpHelper::CharBuffer* m_PipeName;

    CUlpLog* _Log;

    const DWORD ConnectTimeoutDefault = 30; // Default timeout (in seconds) for connecting to LPSpooler
    const DWORD ConnectTryMaxCount = 2;

    DWORD m_ThreadId;

    //Vars to be released/freed
    PROCESS_INFORMATION m_SpoolerProcessInfo;

    ulpHelper::CharBuffer* m_SpoolerExeFullname;

    HANDLE m_PipeHandle;

    bool StartProcessAsCurrentUser(ulpHelper::CharBuffer* commandLine);
    //void StartSpoolerProcessAsUser(bool& spoolerProcessCreated, ulpHelper::CharBuffer* cmdLine);
    bool StartSpoolerProcessAsUser(ulpHelper::CharBuffer* cmdLine);

    // Starting the spooler process
    void StartSpooler(long _lDriverJobId);
    bool m_SpoolerIsDisabledDueToAnError = false;

    //Connecting to the spooler server
    void Connect();
    bool TryConnect(bool* accessDenied);

    void CreatePipename(DWORD printingApplicationsProcessId);
    void InitAndStartSpooler(long _lDriverJobId);

    void CleanSpoolerResources();
    void CleanSpoolerProcessInfo();
    void CleanResources();


public:  
    
    CUlpSpoolerPipe(long _lDriverJobId, CUlpLog* log)
    {
        _Log = log;
        InitAndStartSpooler(_lDriverJobId);
    }

    ~CUlpSpoolerPipe(void)
    {
        CleanResources();
    }

    DWORD WriteToSpoolerPipe(const char* buffer, DWORD bytesToWrite, DWORD* _dwWritePipeLastError);
};