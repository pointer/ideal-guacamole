//  Copyright  UniCredit Services S.C.p.A.
//
//  FILE:      ulpHelper.h
//
//  PURPOSE:   Header for helper functions and definition of class ulphelper::CharBuffer
//

#include <Windows.h>
#include <winnt.h>
#include <tchar.h>
#include <winBase.h>
#include <processthreadsapi.h>
#include "..\..\CommonCSources\ulpLog.h"
#include "..\..\CommonCSources\ulpCharBuffer.h"

namespace ulphelper
{

    // Reg-Key (in HKLM and HKCU) for config-values
    static const LPCTSTR REGKEY_LogPrint2 = _T("SOFTWARE\\IPGM\\UniLogoPrint2");

    static LPCTSTR REGVALUE_ShowConsoleWindows = _T("ShowConsoleWindows");
    static LPCTSTR REGVALUE_ShowAlertOnPluginInit = _T("ShowAlertOnPluginInit");
    static LPCTSTR REGVALUE_LogFolder = _T("LogFolder");


    CharBuffer* ReadLogoPrintRegStrLog(HKEY hKey, LPCTSTR valueName);
    CharBuffer* ReadLogoPrintRegStr(HKEY hKey, LPCTSTR valueName);
    CharBuffer* ReadRegStr(HKEY hKey, LPCTSTR key, LPCTSTR valueName);

    DWORD ReadLogoPrintRegInt(HKEY hKey, LPCTSTR valueName, DWORD defaultValue);
    DWORD ReadLogoPrintRegInt(HKEY hKey, LPCTSTR valueName, DWORD defaultValue, bool *success);
    DWORD ReadRegInt(HKEY hKey, LPCTSTR key, LPCTSTR valueName, DWORD defaultValue, bool *success);
    DWORD GetIntRegHKCUHKLM(const TCHAR* regItemName, int defaultValue, char* logVarname);

    bool GetShowConsoleWindows();

    void NewFunction(TCHAR*& logFolder);

    void GetTempFilename(TCHAR fullname[], int bufferLength, const TCHAR* filenamepart, const TCHAR* extension);

    int StartProcessAndWait(char* appFullname, char* optionFlags, char* filename, PPROCESS_INFORMATION paramPProcessInfo);

#ifdef READER_PLUGIN
    
    bool DoEvents();

#endif // READER_PLUGIN



}
