//  Copyright  UniCredit S.p.A.
//
//  FILE:      ulpHelperUsingLog.h
//
//  PURPOSE:   Header for helper functions and definition of namespace ulpHelper functions using CUlpLog
//
#pragma once 

#include <Windows.h>
#include <tchar.h>
#include "ulpHelper.h"
#include "CUlpLog.h"
#include "ulpCharBuffer.h"

namespace ulpHelper
{

    CharBuffer* ReadLogoPrintRegStrLog(HKEY hKey, LPCTSTR valueName, CUlpLog* log);
    DWORD GetIntRegHKCUHKLM(const TCHAR* regItemName, int defaultValue, char* logVarname, CUlpLog* log);

    bool GetShowConsoleWindows(CUlpLog* log);

}
