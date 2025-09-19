#include <string>
#include <fstream>
#include "..\..\CommonCSources\ulpHelper.h"
#include "..\..\CommonCSources\ulpCharBuffer.h"

namespace ulphelper
{

    static int m_ShowConsoleWindows = -1;

    CharBuffer* ReadLogoPrintRegStrLog(HKEY hKey, LPCTSTR valueName)
    {
        CharBuffer* result = ReadRegStr(hKey, REGKEY_LogPrint2, valueName);
        if (result)
        {
            ulplog::LogVar(valueName, result->GetBufferAnsi());
        }
        else
        {
            char* strkey = "";
            if (hKey == HKEY_LOCAL_MACHINE)
            {
                    strkey = "HKLM";
            }
            if (hKey == HKEY_CURRENT_USER) 
            {
                    strkey = "HKCU";
            }
            ulplog::LogLineParts("Could not read mandatory reg-value ", strkey, "\\", REGKEY_LogPrint2, "\\", valueName, "!", NULL);
            ulplog::LogFlush();
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

        if (hKey == NULL)
        {
            hKey = HKEY_LOCAL_MACHINE;
        }

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
        HKEY regHandle = NULL;
        if (hKey == NULL)
        {
            hKey = HKEY_LOCAL_MACHINE;
        }

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

    bool GetShowConsoleWindows()
    {
        if (m_ShowConsoleWindows == -1)
        {
            int showConsoleWindowsRegVal = ulphelper::ReadLogoPrintRegInt(HKEY_CURRENT_USER, _T("ShowConsoleWindows"), 0);
            if (showConsoleWindowsRegVal == 0)
            {
                m_ShowConsoleWindows = 0;
            }
            else
            {
                m_ShowConsoleWindows = 1;
            }
            ulplog::LogVar("ShowConsoleWindows", m_ShowConsoleWindows ? "true" : "false");
        }
        return m_ShowConsoleWindows == 1;

    }

    DWORD GetIntRegHKCUHKLM(const TCHAR* regItemName, int defaultValue, char* logVarname)
    {
        DWORD iResult = defaultValue;
        const char* valueSource = "default from code";
        bool success = false;
        int userValue = ulphelper::ReadLogoPrintRegInt(HKEY_CURRENT_USER, regItemName, 0, &success);
        if (success && userValue > 0)
        {
            iResult = userValue;
            valueSource = "from HKCU";
        }
        else
        {
            int machineValue = ulphelper::ReadLogoPrintRegInt(HKEY_LOCAL_MACHINE, regItemName, 0, &success);
            if (success && machineValue > 0)
            {
                iResult = machineValue;
                valueSource = "from HKLM";
            }
        }
        char timeoutString[100];
        sprintf_s(timeoutString, sizeof(timeoutString), "%d sec (%s)", iResult, valueSource);
        LPLOGVAR(logVarname, timeoutString);
        return iResult;
    }

    CharBuffer* GetLogFolder(HKEY hKey)
    {
        CharBuffer* result = NULL;
        CharBuffer* logFolderReg = ReadLogoPrintRegStr(hKey, REGVALUE_LogFolder);
        if (logFolderReg != NULL && logFolderReg->Buffer() != NULL)
        {
            DWORD ftyp = GetFileAttributes(logFolderReg->Buffer());
            bool isDirectory = (ftyp != INVALID_FILE_ATTRIBUTES) && ((ftyp & FILE_ATTRIBUTE_DIRECTORY) != 0);
            if (isDirectory)
            {
                result = logFolderReg;
            }
        }
        if (result == NULL && logFolderReg != NULL)
        {
            logFolderReg->free();
        }
        return result;
    }

    void GetTempFilename(TCHAR buffer[], int bufferLength, const TCHAR* filenamepart, const TCHAR* extension)
    {
        TCHAR tempFolder[MAX_PATH + 1];
        TCHAR* logFolder = NULL;


        CharBuffer* cblogFolder = GetLogFolder(HKEY_CURRENT_USER);
        if (cblogFolder == NULL)
        {
            cblogFolder = GetLogFolder(HKEY_LOCAL_MACHINE);
        }

        if (cblogFolder != NULL)
        {
            logFolder = cblogFolder->Buffer();
        }


        if (logFolder == NULL && GetTempPath(_countof(tempFolder), tempFolder) != 0)
        {
            logFolder = tempFolder;
        }

        if (logFolder == NULL)
        {
            logFolder = _T("C:\\Temp");
        }

        _SYSTEMTIME systemTime;
        GetLocalTime(&systemTime);

        int randomNumber = rand();
        _stprintf_s(buffer, bufferLength, _T("%s\\%s(%d)_%04d%02d%02d_%02d%02d%02d_%03d%d.%s"), logFolder, 
                    filenamepart, GetProcessIdOfThread(GetCurrentThread()),
                    systemTime.wYear, systemTime.wMonth, systemTime.wDay, 
                    systemTime.wHour, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds,
                    randomNumber, extension);


        if (cblogFolder != NULL)
        {
            cblogFolder->free();
        }
    }



#ifdef READER_PLUGIN

    bool DoEvents()
    {
        MSG msg;
        BOOL result;
        bool didProcessMessages = false;

        while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            didProcessMessages = true;
            result = GetMessage(&msg, NULL, 0, 0);
            if (result == 0) // WM_QUIT
            {
                PostQuitMessage(msg.wParam);
                break;
            }
            else if (result == -1)
            {
                // Handle errors/exit application, etc.
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        return didProcessMessages;
    }

    int StartProcessAndWait(char* appFullname, char* optionFlags, char* filename, PPROCESS_INFORMATION paramPProcessInfo)
    {
        int result = -1;
        bool bWait;
        PROCESS_INFORMATION processInfo;
        PPROCESS_INFORMATION pProcessInfo;
        if (paramPProcessInfo == NULL)
        {
            pProcessInfo = &processInfo;
            bWait = true;
        }
        else
        {
            // paramPProcessInfo is passed as parameter -> caller will implement wait logic
            bWait = false;
            pProcessInfo = paramPProcessInfo;
        }
        ZeroMemory(pProcessInfo, sizeof(pProcessInfo));

        DWORD lastError = 0;
        LPLOGLINE("Starting process ...");
        LPLOGVAR("Application", appFullname);
        LPLOGVAR("OptionFlags", optionFlags);
        LPLOGVAR("Filename", filename);

        if (m_ShowConsoleWindows == -1)
        {
            m_ShowConsoleWindows = ReadLogoPrintRegInt(HKEY_CURRENT_USER, REGVALUE_ShowConsoleWindows, 0);
            ulplog::LogVar("ShowConsoleWindows", m_ShowConsoleWindows == 0 ? "False" : "True");
        }

        STARTUPINFO startupInfo;
        ZeroMemory(&startupInfo, sizeof(startupInfo));
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.dwFlags = STARTF_USESHOWWINDOW;
        startupInfo.wShowWindow = m_ShowConsoleWindows ? SW_SHOWNORMAL : SW_HIDE;
        DWORD dwCreationFlags = NORMAL_PRIORITY_CLASS;

        char cmdLine[4096];
        ZeroMemory(&cmdLine, sizeof(cmdLine));
        if (filename == NULL)
        {
            sprintf_s(cmdLine, sizeof(cmdLine), "\"%s\" %s %d", appFullname, optionFlags, GetProcessIdOfThread(GetCurrentThread()));
        }
        else
        {
            sprintf_s(cmdLine, sizeof(cmdLine), "\"%s\" %s %d \"%s\"", appFullname, optionFlags, GetProcessIdOfThread(GetCurrentThread()), filename);
        }
        LPLOGVAR("CmdLine", cmdLine);

        SetLastError(0);
        bool isOk = (CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, dwCreationFlags, NULL, NULL, &startupInfo, pProcessInfo) != FALSE);
        if (!isOk)
        {
            lastError = GetLastError();
            ulplog::LogErrorWarning("Could not create process!");
            ulplog::LogVarL("LastError", lastError);
            ulplog::LogFlush();
        }
        else
        {
            if (bWait)
            {
                DWORD dwExitCode = 0;
                SetLastError(0);
                bool waitForProcess = true;
                int level = LPLOGENTERSECTION("Waiting till process ends");
                do
                {
                    SetLastError(0);
                    DWORD w = WaitForSingleObject(pProcessInfo->hProcess, 0);
                    if (w == WAIT_OBJECT_0)
                    {
                        LPLOGLINE("WaitForSingleObject signalled termination of process ...")
                            waitForProcess = false;
                    }
                    else if (w == WAIT_FAILED)
                    {
                        lastError = GetLastError();
                        ulplog::LogErrorWarning("WaitForSingleObject failed!");
                        ulplog::LogVarL("LastError", lastError);
                        ulplog::LogFlush();
                        waitForProcess = false;
                    }
                    else
                    {
                        bool didProcessMessages = DoEvents();
                        DWORD sleepTime = (didProcessMessages ? 0 : 10);
                        Sleep(sleepTime);
                    }
                } while (waitForProcess);
                LPLOGEXITSECTION(level);

                SetLastError(0);
                if (GetExitCodeProcess(pProcessInfo->hProcess, &dwExitCode) != 0)
                {
                    result = dwExitCode;
                }
                else
                {
                    result = -2;
                    lastError = GetLastError();
                    ulplog::LogErrorWarning("GetExitCodeProcess failed!");
                    ulplog::LogVarL("LastError", lastError);
                    ulplog::LogFlush();
                    lastError = GetLastError();
                }

                CloseHandle(pProcessInfo->hProcess);
                CloseHandle(pProcessInfo->hThread);
            }
            else
            {
                //Caller just wanted to start the process
                result = 0;
            }

        }

        return result;
    }

#endif // READER_PLUGIN

}
