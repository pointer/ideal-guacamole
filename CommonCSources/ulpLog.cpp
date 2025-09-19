#ifdef READER_PLUGIN
#include <PIHeaders.h>
#endif // READER_PLUGIN

#include <fstream>
#include "ulpLog.h"
#include "ulpHelper.h"
#include <Windows.h>
#include <winnt.h>
#include <winBase.h>
#include <ctime>
#include <cstdarg>

namespace ulplog
{

    const int MAXSECTIONS = 20;
    const int INDENTSPACES = 4;

    void LogCurrentTime();
    void MakeIndent();
    void SetIndentPrefix(int index);

    static bool bLogInitialized;
    static CRITICAL_SECTION m_CS_Log;

    typedef struct SectionData
    {
        clock_t startTime;
        const char* text;
    } SectionData;

    const int MAXTHREADCOUNT = 3;
    SectionData sections[MAXTHREADCOUNT+1][MAXSECTIONS*INDENTSPACES+11];
    int indentLevel[MAXTHREADCOUNT+1] = { 0, 0, 0, 0};
    char m_Indent[MAXTHREADCOUNT+1][1000];

    std::ofstream Log;

    char* INSERTTIME;

    DWORD m_MainThreadId = 0;
    DWORD m_MsgloopThreadId = 0;    //Currently not used
    DWORD m_ConnectThreadId = 0;    //Currently not used


    int GetThreadIndex()
    {
        int index = 0;
        DWORD pid = GetCurrentThreadId();
        if (pid == m_MainThreadId)
        {
            index = 1;
        }
        else if (pid == m_MsgloopThreadId)
        {
            index = 2;
        }
        else if (pid == m_ConnectThreadId)
        {
            index = 3;
        }
        return index;
    }

    char* GetIndent()
    {
        int index = GetThreadIndex();
        SetIndentPrefix(index);
        return m_Indent[index];
    }

    SectionData* GetSectionData(int i)
    {
        return &sections[GetThreadIndex()][i];
    }


    int GetIndentLevel()
    {
        return indentLevel[GetThreadIndex()];
    }

    void SetIndentLevel(int level)
    {
        indentLevel[GetThreadIndex()] = level;
    }

    void SetIndentPrefix(int index)
    {
        index = index > 0 ? index : GetThreadIndex();
        char* indent = m_Indent[index];
        switch (index)
        {
            case 0: // unknown ThreadId
                indent[0] = '?';
                indent[1] = '?';
                break;
            case 1: // plugin thread
                indent[0] = ' ';
                indent[1] = ' ';
                break;
            case 2: // message thread
                indent[0] = 'm';
                indent[1] = 'T';
                break;
            case 3: // connect thread
                indent[0] = 'c';
                indent[1] = 'T';
                break;
        }
    }

    //Currently not used, but may be useful in future
    //void LogRegisterMessageloopThread()
    //{
    //    m_MsgloopThreadId = GetCurrentThreadId();
    //}

    //void LogRegisterConnectThread()
    //{
    //    m_ConnectThreadId = GetCurrentThreadId();
    //}


    void LogCurrentTime()
    {
        if (!bLogInitialized) return;
        try {

            char buffer[80];
            ZeroMemory(&buffer, sizeof(buffer));

            SYSTEMTIME systemTime;
            GetSystemTime(&systemTime);
            sprintf_s(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d", 
                      systemTime.wYear, systemTime.wMonth, systemTime.wDay,
                      systemTime.wHour, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds);
            Log << buffer;
        } catch (...) {}
    }

    void WriteToStream(const char* cptr, bool prependIndent, const char* prefixPos0, const char* prefix)
    {
        while (*cptr != '\0')
        {
            if (*cptr == '\r' || *cptr == '\n')
            {
                if (*cptr == '\r') 
                {
                    cptr++;
                    if (*cptr == '\n') cptr++;
                }
                else
                {
                    cptr++;
                }
                Log << "\n";
                if (prefixPos0 != NULL) ulplog::Log << prefixPos0;
                if (prependIndent) Log << GetIndent();
                if (prefix != NULL) Log << prefix;
            }
            else 
            {
                Log << *cptr;
                cptr++;
            }
        }
    }

    void WriteToStream(const char* text, bool prependIndent, const char* prefixPos0, const char* prefix, bool appendNewline, bool flush, bool append)
    {
        if (!append)
        {
            if (prefixPos0 != NULL) ulplog::Log << prefixPos0;
            if (prependIndent) ulplog::Log << GetIndent();
            if (prefix != NULL) ulplog::Log << prefix;
        }

        WriteToStream(text, prependIndent, prefixPos0, prefix);

        if (appendNewline) Log << '\n';
        if (flush) Log.flush();
    };
    
    void WriteToStream(const char* text, bool prependIndent, const char* prefixPos0, const char* prefix, bool appendNewline, bool flush)
    {
        WriteToStream(text, prependIndent, prefixPos0, prefix, appendNewline, flush, false);
    }

    void WriteToStream(const char* text, bool prependIndent, bool appendNewline, bool flush)
    {
        WriteToStream(text, prependIndent, NULL, NULL, appendNewline, flush);
    }

    void LogLineParts(char* text, ...)
    {
        if (!bLogInitialized) return;
        va_list vl;

        va_start(vl, text);

        EnterCriticalSection(&m_CS_Log);
        try 
        {
            const char* indent = GetIndent();
            Log << indent;
            const char* t = text;
            for (;;) {
                if (t==NULL) break;
                if (t==INSERTTIME)
                {
                    LogCurrentTime();
                }
                else
                {
                    WriteToStream(t, true, NULL, "--->");
                    //Log << t;
                }
                t = va_arg(vl, char*);
            }
            Log << '\n';
        } catch (...) {}
        LeaveCriticalSection(&m_CS_Log);

        va_end(vl);
    }


    void LogFlush()
    {
        if (!bLogInitialized) return;
        Log.flush();
    }

    void LogLine(const char* text)
    {
        if (!bLogInitialized) return;
        EnterCriticalSection(&m_CS_Log);
        try 
        {
            SetIndentPrefix(0);
            WriteToStream(text, true, true, false);
        } catch (...) {}
        LeaveCriticalSection(&m_CS_Log);
    }

    void LogLineNoIdent(const char* text)
    {
        if (!bLogInitialized) return;
        EnterCriticalSection(&m_CS_Log);
        try 
        {
            SetIndentPrefix(0);
            WriteToStream(text, false, true, false);
        } catch (...) {}
        LeaveCriticalSection(&m_CS_Log);
    }

    void LogLineFlush(const char* text)
    {
        if (!bLogInitialized) return;
        EnterCriticalSection(&m_CS_Log);
        try 
        {
            SetIndentPrefix(0);
            WriteToStream(text, true, true, true);
        } catch (...) {}
        LeaveCriticalSection(&m_CS_Log);
    }

    void LogError(const char* text, std::exception e)
    {
        if (!bLogInitialized) return;
        EnterCriticalSection(&m_CS_Log);
        try 
        {
            SetIndentPrefix(0);
            WriteToStream(text, true, "!!! ", NULL, false, false);
            Log << ": ";
            WriteToStream((char *)e.what(), true, "! ! ", NULL, true, true);
        } catch (...) {}
        LeaveCriticalSection(&m_CS_Log);
    }

    void LogErrorWarning(const char* text)
    {
        if (!bLogInitialized) return;
        EnterCriticalSection(&m_CS_Log);
        char* indent = GetIndent();
        try 
        {
            indent[0] = '!';
            indent[1] = '!';
            indent[2] = '!';
            WriteToStream(text, true, NULL, NULL, true, true);
        } catch (...) {}
        indent[2] = ' ';
        LeaveCriticalSection(&m_CS_Log);
    }

    void LogVar(const wchar_t* name, const char* value)
    {
        if (!bLogInitialized) return;
        EnterCriticalSection(&m_CS_Log);
        try 
        {
            char* indent = GetIndent();
            Log << indent << name << ": ";
            WriteToStream(value, false, NULL, NULL, true, false);
        } catch (...) {}
        LeaveCriticalSection(&m_CS_Log);
    }

    void LogVar(const char *name, const char* value)
    {
        if (!bLogInitialized) return;
        EnterCriticalSection(&m_CS_Log);
        try
        {
            char* indent = GetIndent();
            Log << indent << name << ": ";
            WriteToStream(value, false, NULL, NULL, true, false);
        }
        catch (...) {}
        LeaveCriticalSection(&m_CS_Log);
    }

    void LogVarL(const char* name, signed __int64 value)
    {
        if (!bLogInitialized) return;
        EnterCriticalSection(&m_CS_Log);
        try
        {
            char* indent = GetIndent();
            Log << indent << name << ": " << value;
            WriteToStream("", false, NULL, NULL, true, false);
        }
        catch (...) {}
        LeaveCriticalSection(&m_CS_Log);
    }

    void LogVarUL(const char* name, unsigned __int64 value)
    {
        if (!bLogInitialized) return;
        EnterCriticalSection(&m_CS_Log);
        try
        {
            char* indent = GetIndent();
            Log << indent << name << ": " << value;
            WriteToStream("", false, NULL, NULL, true, false);
        }
        catch (...) {}
        LeaveCriticalSection(&m_CS_Log);
    }

    void InitIndent(int i)
    {
        m_Indent[i][0] = ' ';
        m_Indent[i][1] = ' ';
        m_Indent[i][2] = ' ';
        m_Indent[i][3] = ' ';
        m_Indent[i][4] = '\0';
    }

    void InitLog(TCHAR * fileNamePart)
    {
        try 
        {
#ifdef READER_PLUGIN
            int showAlertOnPluginInit = ulphelper::ReadLogoPrintRegInt(HKEY_CURRENT_USER, ulphelper::REGVALUE_ShowAlertOnPluginInit, 0);
            if (showAlertOnPluginInit != 0)
            {
                AVAlertNote("Plugin Init (to debug)");
                ulphelper::DoEvents();
            }
#endif //READER_PLUGIN

            m_MainThreadId = GetCurrentThreadId();
            INSERTTIME = "###TIME###";
            InitializeCriticalSection(&m_CS_Log);

            ZeroMemory(&sections, sizeof(sections));

            ZeroMemory(m_Indent, sizeof(m_Indent));
            for (int i=0; i <= MAXTHREADCOUNT; i++) InitIndent(i);

            TCHAR logFileName[MAX_PATH + 1];
            ZeroMemory(logFileName, sizeof(logFileName));

            ulphelper::GetTempFilename(logFileName, _countof(logFileName), fileNamePart, _T("txt"));
            Log.open(logFileName, std::ios_base::out | std::ios_base::trunc);
            Log.flush();

            bLogInitialized = true;
        } catch (...) {}

    }

    void TerminateLog()
    {
        try 
        {
            bLogInitialized = false;
            Log.flush();
            Log.close();
        } catch (...) {}
        DeleteCriticalSection(&m_CS_Log);
    }

    int EnterSection(const char* text)
    {
        int iLevel = GetIndentLevel();
        if (!bLogInitialized) return iLevel;
        EnterCriticalSection(&m_CS_Log);
        try 
        {
            char* indent = GetIndent();
            if (text == NULL || strnlen_s(text, 100) > 90) text = "???";
            Log << indent << "Enter Section '" << text << "' (";
            LogCurrentTime();
            Log << ")\n";
            Log.flush();
            SectionData* sd = GetSectionData(iLevel);
            sd->startTime = clock();
            sd->text = text;
            SetIndentLevel(iLevel + 1);
            MakeIndent();
        } catch (...) {}
        LeaveCriticalSection(&m_CS_Log);
        return iLevel;
    }

    void ExitSection(int level)
    {
        if (!bLogInitialized) return;
        EnterCriticalSection(&m_CS_Log);
        try {
            int iLevel = level >= 0 ? level : 0;
            SetIndentLevel(iLevel);
            MakeIndent();
            char* indent = GetIndent();

            SectionData* sd = GetSectionData(iLevel);
            if (sd != NULL && sd->text != NULL)
            {
                Log << indent << "Exit Section '" << sd->text << "' (";

                double CLOCKS_PER_mSEC = (CLOCKS_PER_SEC / 1000);
                if (sd->startTime != 0)
                {
                    int ticks = (int)((clock() - sd->startTime) / CLOCKS_PER_mSEC);
                    Log << ticks << "ms)\n";
                }
                else 
                {
                    Log << "???)\n";
                }
            }
            else
            {
                Log << indent << "Exit Section '???'\n";
            }
            Log.flush();
        } catch (...) {}
        LeaveCriticalSection(&m_CS_Log);
    }

    void MakeIndent()
    {
        int i;
        char* indent = GetIndent();
        int iLevel = GetIndentLevel();
        int level = (iLevel < MAXSECTIONS ? iLevel : MAXSECTIONS);
        int length = level * INDENTSPACES + 4;
        for (i = 4; i < length; i++) indent[i] = ' ';
        indent[i] = '\0';
    }

    void LogLastErrorMessage(const char* text, bool bIndent, bool bErrorOnly)
    {
        if (!bLogInitialized) return;
        EnterCriticalSection(&m_CS_Log);
        try 
        {
            char* indent = GetIndent();

            HRESULT hresult = GetLastError();
            LPTSTR errorText = NULL;

            FormatMessage(
                   // use system message tables to retrieve error text
                   FORMAT_MESSAGE_FROM_SYSTEM
                   // allocate buffer on local heap for error text
                   |FORMAT_MESSAGE_ALLOCATE_BUFFER
                   // Important! will fail otherwise, since we're not 
                   // (and CANNOT) pass insertion parameters
                   |FORMAT_MESSAGE_IGNORE_INSERTS,  
                   NULL,    // unused with FORMAT_MESSAGE_FROM_SYSTEM
                   hresult,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPTSTR)&errorText,  // output 
                   0, // minimum size for output buffer
                   NULL);   // arguments - see note 

            if (errorText != NULL && (!bErrorOnly || hresult!=0))
            {
                if (text != NULL)
                {
                    Log << (bIndent ? indent : "") << text << "\n";
                }

                Log << (bIndent ? indent : "") << "!!! Error (" << hresult << "): ";
                WriteToStream((char*)errorText, true, NULL, "!!! ", true, true, true);
                errorText = NULL;
            }    
        } catch (...) {}
        LeaveCriticalSection(&m_CS_Log);
    }

#ifdef READER_PLUGIN

    void LogAdobeError(const char* text, ASInt32 error)
    {
        EnterCriticalSection(&m_CS_Log);
        char* indent = GetIndent();
        try 
        {
            indent[0] = '!';
            indent[1] = '!';
            indent[2] = '!';

            Log << indent << "!!! Adobe Error 0x";
            Log << std::hex << error;
            Log << " " << text << "\n";

        } catch (...) {}
        indent[2] = ' ';
        LeaveCriticalSection(&m_CS_Log);
    }

#endif
}