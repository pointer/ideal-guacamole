#include "ulpHelperUsingLog.h"

namespace ulpHelper
{

    static int m_ShowConsoleWindows = -1;

    CharBuffer* ReadLogoPrintRegStrLog(HKEY hKey, LPCTSTR valueName, CUlpLog* log)
    {
        CharBuffer* result = ReadRegStr(hKey, REGKEY_LogPrint2, valueName);
        if (result)
        {
            if (log != NULL) {
                log->LogVar(valueName, result->GetBufferAnsi());
            }
        }
        else
        {
            char* strkey = nullptr;
            if (hKey == HKEY_LOCAL_MACHINE)
            {
                    auto strTemp = "HKLM";
                    strkey = new char[strlen(strTemp) + 1];
                    strcpy_s(strkey, strlen(strTemp) + 1, strTemp);
                    //strkey = "HKLM";
            }
            if (hKey == HKEY_CURRENT_USER) 
            {
                auto strTemp = "HKCU";
                strkey = new char[strlen(strTemp) + 1];
                strcpy_s(strkey, strlen(strTemp) + 1, strTemp);
                //strkey = "HKCU";
            }
            if (log != NULL) {
                log->LogLineParts(const_cast<char*>("Could not read mandatory reg-value "), strkey, "\\", REGKEY_LogPrint2, "\\", valueName, "!", NULL);
                log->LogFlush();
            }
        }
        return result;
    }

    CharBuffer* ReadLogoPrintRegStr(HKEY hKey, LPCTSTR valueName)
    {
        return ReadRegStr(hKey, REGKEY_LogPrint2, valueName);
    }

    CharBuffer* ReadRegStr(HKEY hKey, LPCTSTR key, LPCTSTR valueName)
    {
        CharBuffer* result = NULL;
        HKEY regHandle = NULL;
        if (hKey == 0) return NULL;

        REGSAM samDesired = KEY_QUERY_VALUE;
        if (hKey == HKEY_LOCAL_MACHINE) 
        {
            samDesired |= KEY_WOW64_64KEY;
        }

        if (RegOpenKeyEx(hKey, key, 0, samDesired, &regHandle) != ERROR_SUCCESS)
        {
            result = NULL;
        }
        else
        {
            DWORD dwType = REG_SZ | REG_EXPAND_SZ;
            DWORD dwCount = 0;

            LSTATUS rc = RegQueryValueEx(regHandle, valueName, NULL, &dwType, NULL, &dwCount);
            if (rc == ERROR_SUCCESS && dwCount > 0)
            {
                result = new CharBuffer(dwCount / sizeof(TCHAR));
                rc = RegQueryValueEx(regHandle, valueName, NULL, &dwType,(LPBYTE) result->Buffer(), &dwCount);
            }
            else if (rc == ERROR_FILE_NOT_FOUND)
            {
                result = NULL;
            }
            else 
            {
                result = new CharBuffer(0);
            }
        }

        if (regHandle) 
        {
                RegCloseKey(regHandle);
        }
        return result;
    }

    DWORD ReadLogoPrintRegInt(HKEY hKey, LPCTSTR valueName, DWORD defaultValue)
    {
        bool success;
        return ReadRegInt(hKey, REGKEY_LogPrint2, valueName, defaultValue, &success);
    }

    DWORD ReadLogoPrintRegInt(HKEY hKey, LPCTSTR valueName, DWORD defaultValue, bool *success)
    {
        return ReadRegInt(hKey, REGKEY_LogPrint2, valueName, defaultValue, success);
    }

    DWORD ReadRegInt(HKEY hKey, LPCTSTR key, LPCTSTR valueName, DWORD defaultValue, bool *success)
    {
        DWORD result = defaultValue;
        *success = false;
        if (hKey == 0) return 0;

        HKEY regHandle = NULL;
        REGSAM samDesired = KEY_QUERY_VALUE;
        if (hKey == HKEY_LOCAL_MACHINE)
        {
            samDesired |= KEY_WOW64_64KEY;
        }

        if(RegOpenKeyEx(hKey, key, 0, samDesired, &regHandle) == ERROR_SUCCESS)
        {
            DWORD dwType = REG_DWORD;
            DWORD dwCount = 0;

            LSTATUS rc = RegQueryValueEx(regHandle, valueName, NULL, &dwType, NULL, &dwCount);
            if (rc == ERROR_SUCCESS && dwCount == 4)
            {
                rc = RegQueryValueEx(regHandle, valueName, NULL, &dwType,(LPBYTE) &result, &dwCount);
                *success = (rc == ERROR_SUCCESS);
            }
        }

        if (regHandle)
        {
            RegCloseKey(regHandle);
        }
        return result;
    }

    bool GetShowConsoleWindows(CUlpLog* log)
    {
        if (m_ShowConsoleWindows == -1)
        {
            int showConsoleWindowsRegVal = ReadLogoPrintRegInt(HKEY_CURRENT_USER, _T("ShowConsoleWindows"), 0);
            if (showConsoleWindowsRegVal == 0)
            {
                m_ShowConsoleWindows = 0;
            }
            else
            {
                m_ShowConsoleWindows = 1;
            }
            if (log != NULL) {
                log->LogVar("ShowConsoleWindows", m_ShowConsoleWindows ? "true" : "false");
            }
        }
        return m_ShowConsoleWindows == 1;

    }

    DWORD GetIntRegHKCUHKLM(const TCHAR* regItemName, int defaultValue, char* logVarname, CUlpLog* log)
    {
        DWORD iResult = defaultValue;
        const char* valueSource = "default from code";
        bool success = false;
        int userValue = ReadLogoPrintRegInt(HKEY_CURRENT_USER, regItemName, 0, &success);
        if (success && userValue > 0)
        {
            iResult = userValue;
            valueSource = "from HKCU";
        }
        else
        {
            int machineValue = ReadLogoPrintRegInt(HKEY_LOCAL_MACHINE, regItemName, 0, &success);
            if (success && machineValue > 0)
            {
                iResult = machineValue;
                valueSource = "from HKLM";
            }
        }
        char timeoutString[100];
        sprintf_s(timeoutString, sizeof(timeoutString), "%d sec (%s)", iResult, valueSource);
        if (log != NULL) {
            log->LogVar(logVarname, timeoutString);
        }
        return iResult;
    }





}
