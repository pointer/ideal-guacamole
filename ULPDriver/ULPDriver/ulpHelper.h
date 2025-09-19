//  Copyright  UniCredit S.p.A.
//
//  FILE:      ulpHelper.h
//
//  PURPOSE:   Header for helper functions and definition of class ulpHelper::CharBuffer
//
#pragma once 

#include <Windows.h>
#include <tchar.h>
#include "ulpCharBuffer.h"

namespace ulpHelper
{

    // Reg-Key (in HKLM and HKCU) for config-values
    static const LPCTSTR REGKEY_LogPrint2 = _T("SOFTWARE\\IPGM\\UniLogoPrint2");

    static LPCTSTR REGVALUE_ShowConsoleWindows = _T("ShowConsoleWindows");
    static LPCTSTR REGVALUE_ShowAlertOnPluginInit = _T("ShowAlertOnPluginInit");
    static LPCTSTR REGVALUE_LogFolder = _T("LogFolder");



    CharBuffer* ReadLogoPrintRegStr(HKEY hKey, LPCTSTR valueName);
    CharBuffer* ReadRegStr(HKEY hKey, LPCTSTR key, LPCTSTR valueName);

    DWORD ReadLogoPrintRegInt(HKEY hKey, LPCTSTR valueName, DWORD defaultValue);
    DWORD ReadLogoPrintRegInt(HKEY hKey, LPCTSTR valueName, DWORD defaultValue, bool *success);
    DWORD ReadRegInt(HKEY hKey, LPCTSTR key, LPCTSTR valueName, DWORD defaultValue, bool *success);

}
