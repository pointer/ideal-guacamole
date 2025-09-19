#include <fstream>
#include <Windows.h>
#include <ctime>
#include <cstdarg>
#include "CUlpLog.h"
#include "ulpHelper.h"



    void CUlpLog::LogCurrentTime()
    {
        if (!m_bLogInitialized) return;
        try {

            char buffer[80];
            ZeroMemory(&buffer, sizeof(buffer));

            SYSTEMTIME systemTime;
            GetSystemTime(&systemTime);
            sprintf_s(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d", 
                      systemTime.wYear, systemTime.wMonth, systemTime.wDay,
                      systemTime.wHour, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds);
            m_Log << buffer;
        } catch (...) {}
    }



    void CUlpLog::LogLineParts(char* text, ...)
    {
        if (!m_bLogInitialized) return;
        va_list vl;

        va_start(vl, text);
        std::unique_lock<std::mutex> ul(mutex_);
        //EnterCriticalSection(&m_CS_Log);
        try 
        {
            const char* indent = GetIndent();
            m_Log << indent;
            const char* t = text;
            for (;;) {
                if (t==NULL) break;
                if (std::strcmp(t , INSERTTIME) == 0)  //if (t==INSERTTIME)
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
            m_Log << '\n';
        } catch (...) {}
        ul.unlock();
        //LeaveCriticalSection(&m_CS_Log);

        va_end(vl);
    }


    void CUlpLog::LogFlush()
    {
        if (!m_bLogInitialized) return;
        m_Log.flush();
    }

    void CUlpLog::LogLine(const char* text)
    {
        if (!m_bLogInitialized) return;
        std::unique_lock<std::mutex> ul(mutex_);
        // EnterCriticalSection(&m_CS_Log);//
        try 
        {
            SetIndentPrefix(0);
            WriteToStream(text, true, true, false);
        } catch (...) {}
        ul.unlock();
    }

    void CUlpLog::LogLineNoIdent(const char* text)
    {
        if (!m_bLogInitialized) return;
        std::unique_lock<std::mutex> ul(mutex_);
        try 
        {
            SetIndentPrefix(0);
            WriteToStream(text, false, true, false);
        } catch (...) {}
        ul.unlock();
    }

    void CUlpLog::LogLineFlush(const char* text)
    {
        if (!m_bLogInitialized) return;
        std::unique_lock<std::mutex> ul(mutex_);
        try 
        {
            SetIndentPrefix(0);
            WriteToStream(text, true, true, true);
        } catch (...) {}
        ul.unlock();
    }

    void CUlpLog::LogError(const char* text, std::exception e)
    {
        if (!m_bLogInitialized) return;
        std::unique_lock<std::mutex> ul(mutex_);
        try 
        {
            SetIndentPrefix(0);
            WriteToStream(text, true, "!!! ", NULL, false, false);
            m_Log << ": ";
            WriteToStream((char *)e.what(), true, "! ! ", NULL, true, true);
        } catch (...) {}
        ul.unlock();
    }

    void CUlpLog::LogErrorWarning(const char* text)
    {
        if (!m_bLogInitialized) return;
        std::unique_lock<std::mutex> ul(mutex_);
        char* indent = GetIndent();
        try 
        {
            indent[0] = '!';
            indent[1] = '!';
            indent[2] = '!';
            WriteToStream(text, true, NULL, NULL, true, true);
        } catch (...) {}
        indent[2] = ' ';
        ul.unlock();
    }

    void CUlpLog::LogVar(const wchar_t* name, const char* value)
    {
        if (!m_bLogInitialized) return;
        std::unique_lock<std::mutex> ul(mutex_);
        try 
        {
            char* indent = GetIndent();

            constexpr auto char_len = sizeof(value);
            char mbstr[char_len] = {};
            std::wcstombs(mbstr, name, 11);
            m_Log << indent << mbstr << ": ";
            //m_Log << indent << T2A(name) << ": ";
            WriteToStream(value, false, NULL, NULL, true, false);
        } catch (...) {}
        ul.unlock();
    }

    void CUlpLog::LogVar(const char *name, const char* value)
    {
        if (!m_bLogInitialized) return;
        std::unique_lock<std::mutex> ul(mutex_);
        try
        {
            char* indent = GetIndent();
            m_Log << indent << name << ": ";
            WriteToStream(value, false, NULL, NULL, true, false);
        }
        catch (...) {}
        ul.unlock();
    }

    void CUlpLog::LogVarL(const char* name, signed __int64 value)
    {
        if (!m_bLogInitialized) return;
        std::unique_lock<std::mutex> ul(mutex_);
        try
        {
            char* indent = GetIndent();
            m_Log << indent << name << ": " << value;
            WriteToStream("", false, NULL, NULL, true, false);
        }
        catch (...) {}
        ul.unlock();
    }

    void CUlpLog::LogVarUL(const char* name, unsigned __int64 value)
    {
        if (!m_bLogInitialized) return;
        std::unique_lock<std::mutex> ul(mutex_);
        try
        {
            char* indent = GetIndent();
            m_Log << indent << name << ": " << value;
            WriteToStream("", false, NULL, NULL, true, false);
        }
        catch (...) {}
        ul.unlock();
    }

    int CUlpLog::EnterSection(const char* text)
    {
        int iLevel = GetIndentLevel();
        if (!m_bLogInitialized) return iLevel;
        std::unique_lock<std::mutex> ul(mutex_);
        try 
        {
            char* indent = GetIndent();
            if (text == NULL || strnlen_s(text, 100) > 90) text = "???";
            m_Log << indent << "Enter Section '" << text << "' (";
            LogCurrentTime();
            m_Log << ")\n";
            m_Log.flush();
            SectionData* sd = GetSectionData(iLevel);
            sd->startTime = clock();
            sd->text = text;
            SetIndentLevel(iLevel + 1);
            MakeIndent();
        } catch (...) {}
        ul.unlock();
        return iLevel;
    }

    void CUlpLog::ExitSection(int level)
    {
        if (!m_bLogInitialized) return;
        std::unique_lock<std::mutex> ul(mutex_);
        try {
            int iLevel = level >= 0 ? level : 0;
            SetIndentLevel(iLevel);
            MakeIndent();
            char* indent = GetIndent();

            SectionData* sd = GetSectionData(iLevel);
            if (sd != NULL && sd->text != NULL)
            {
                m_Log << indent << "Exit Section '" << sd->text << "' (";

                double CLOCKS_PER_mSEC = (CLOCKS_PER_SEC / 1000);
                if (sd->startTime != 0)
                {
                    int ticks = (int)((clock() - sd->startTime) / CLOCKS_PER_mSEC);
                    m_Log << ticks << "ms)\n";
                }
                else 
                {
                    m_Log << "???)\n";
                }
            }
            else
            {
                m_Log << indent << "Exit Section '???'\n";
            }
            m_Log.flush();
        } catch (...) {}
        ul.unlock();
    }


    void CUlpLog::LogLastErrorMessage(const char* text, bool bIndent, bool bErrorOnly)
    {
        if (!m_bLogInitialized) return;
        std::unique_lock<std::mutex> ul(mutex_);
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
                    m_Log << (bIndent ? indent : "") << text << "\n";
                }

                m_Log << (bIndent ? indent : "") << "!!! Error (" << hresult << "): ";
                WriteToStream((char*)errorText, true, NULL, "!!! ", true, true, true);
                errorText = NULL;
            }    
        } catch (...) {}
        ul.unlock();
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
