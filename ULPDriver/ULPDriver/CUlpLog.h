//  Copyright  UniCredit S.p.A.
//
//  FILE:      ulpLog.h
//
//  PURPOSE:   Header for a lightweight logger
//
//

#pragma once 
#include <memory>
#include <stdexcept>
#include <fstream>
#include <tchar.h>
#include <mutex>
#include <thread>
#include <format>
#include <memory>

#include "ulpHelper.h"
#include "ulpCharBuffer.h"

using namespace std::literals;


#define TIME (char *)ulplog::INSERTTIME;

#define LPLOGNEWLINE() ulplog::LogLine("\n");
#define LPLOGLINE(text) ulplog::LogLine(text);
#define LPLOGLINENOIDENT(text) ulplog::LogLineNoIdent(text);
#define LPLOGLINEFLUSH(text) ulplog::LogLineFlush(text);
#define LPLOGERROR(text, e) ulplog::LogError(text, e);
#define LPLOGERRORWARNING(text) ulplog::LogErrorWarning(text);
#define LPLOGLASTERROR(text, bIndent, bErrorOnly) ulplog::LogLastErrorMessage(text, bIndent, bErrorOnly);
#define LPLOGENTERSECTION(text) ulplog::EnterSection(text);
#define LPLOGEXITSECTION(level) ulplog::ExitSection(level);
#define LPLOGVAR(name, value) ulplog::LogVar(name, value);
#define LPLOGVARL(name, value) ulplog::LogVarL(name, value);
#define LPLOGVARUL(name, value) ulplog::LogVarUL(name, value);
#define LPLOGADOBEERROR(text, error) ulplog::LogAdobeError(text, error);


const int MAXTHREADCOUNT = 3;
const int MAXSECTIONS = 20;
const int INDENTSPACES = 4;

class CUlpLog
{

private:
    void LogCurrentTime();

    bool m_bLogInitialized;
    CRITICAL_SECTION m_CS_Log;  // To be freed
    std::mutex mutex_;
    typedef struct SectionData
    {
        clock_t startTime;
        const char* text;
    } SectionData;

    SectionData sections[MAXTHREADCOUNT + 1][MAXSECTIONS * INDENTSPACES + 11];
    int indentLevel[MAXTHREADCOUNT + 1] = { 0, 0, 0, 0 };
    char m_Indent[MAXTHREADCOUNT + 1][1000];

    std::ofstream m_Log;  // To be freed

    DWORD m_MainThreadId;
    DWORD m_MsgloopThreadId;    //Currently not used
    DWORD m_ConnectThreadId;    //Currently not used


public:

    char* INSERTTIME;

    int EnterSection(const char* text);
    void ExitSection(int level);

    void LogLineParts(char* text, ...);

    void LogFlush();
    void LogLine(const char* text);
    void LogLineNoIdent(const char* text);
    void LogLineFlush(const char* text);

    void LogError(const char* text, std::exception e);
    void LogErrorWarning(const char* text);

    void LogLastErrorMessage(const char* text, bool bIndent, bool bErrorOnly);
    void LogVar(const wchar_t* name, const char* value);
    void LogVar(const char* name, const char* value);
    void LogVarL(const char* name, signed __int64 value);
    void LogVarUL(const char* name, unsigned __int64 value);

        CUlpLog(TCHAR* fileNamePart)
        {
            m_MainThreadId = 0;
            m_MsgloopThreadId = 0;    //Currently not used
            m_ConnectThreadId = 0;    //Currently not used

            try
            {
                m_MainThreadId = GetCurrentThreadId();
                auto strTemp = "###TIME###";
                INSERTTIME = new char[strlen(strTemp) + 1];
                strcpy_s(INSERTTIME, strlen(strTemp) + 1, strTemp);
                //INSERTTIME = "###TIME###";
                InitializeCriticalSection(&m_CS_Log);

                ZeroMemory(&sections, sizeof(sections));

                ZeroMemory(m_Indent, sizeof(m_Indent));
                for (int i = 0; i <= MAXTHREADCOUNT; i++) InitIndent(i);

                TCHAR logFileName[MAX_PATH + 1];
                ZeroMemory(logFileName, sizeof(logFileName));

                GetTempFilename(logFileName, _countof(logFileName), fileNamePart, _T("txt"));
                m_Log.open(logFileName, std::ios_base::out | std::ios_base::trunc);
                m_Log.flush();

                m_bLogInitialized = true;
            }
            catch (...) {}
        }

        ~CUlpLog(void)
        {
            try
            {
                m_bLogInitialized = false;
                m_Log.flush();
                m_Log.close();
            }
            catch (...) {}
            DeleteCriticalSection(&m_CS_Log);
        }


        void GetTempFilename(TCHAR buffer[], int bufferLength, const TCHAR* filenamepart, const TCHAR* extension)
        {
            char /* TCHAR */ tempFolder[MAX_PATH + 1];
            char* /* TCHAR* */ logFolder = NULL;
        
            std::unique_ptr<ulpHelper::CharBuffer> cblogFolder = std::make_unique<ulpHelper::CharBuffer>( GetLogFolder(HKEY_CURRENT_USER));
            //ulpHelper::CharBuffer* cblogFolder = GetLogFolder(HKEY_CURRENT_USER);
            if (cblogFolder == NULL)
            {
                cblogFolder = std::make_unique<ulpHelper::CharBuffer>(GetLogFolder(HKEY_LOCAL_MACHINE));
            }
        
            if (cblogFolder != NULL)
            {
                logFolder = (char*) cblogFolder->Buffer();
            }
        
        
            if (logFolder == NULL && GetTempPath(_countof(tempFolder), LPWSTR(tempFolder)) != 0)
            {
                logFolder = tempFolder;
            }
        
            if (logFolder == NULL)
            {
                auto strTemp = "C:\\Temp";
                logFolder = new char[strlen(strTemp) + 1];
                strcpy_s(logFolder, strlen(strTemp) + 1, strTemp);
                //logFolder = _T("C:\\Temp");
            }
        
            _SYSTEMTIME systemTime;
            GetLocalTime(&systemTime);
            auto th_id = std::this_thread::get_id();
            auto randomNumber = rand();
/*
            std::string str = std::format("{:p}\\{:p}({})_{:04}{:02}{:02}_{:02}{:02}{:02}_{:03}{}.{:p}", 
                        logFolder, filenamepart, th_id, 
                        systemTime.wYear, systemTime.wMonth, systemTime.wDay,
                        systemTime.wHour, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds,
                        randomNumber, extension);

            std::copy(str.begin(), str.end(), buffer);
*/
            _stprintf_s(buffer, bufferLength, _T("%s\\%s(%d)_%04d%02d%02d_%02d%02d%02d_%03d%d.%s"), logFolder, 
                        filenamepart, th_id,
                        systemTime.wYear, systemTime.wMonth, systemTime.wDay, 
                        systemTime.wHour, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds,
                        randomNumber, extension);
           
            //if (cblogFolder != NULL)
            //{
            //    cblogFolder->free();
            //}
        }

        ulpHelper::CharBuffer* GetLogFolder(HKEY hKey)
        {
            ulpHelper::CharBuffer* result = NULL;
            ulpHelper::CharBuffer* logFolderReg = ulpHelper::ReadLogoPrintRegStr(hKey, ulpHelper::REGVALUE_LogFolder);
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



        void InitIndent(int i)
        {
            m_Indent[i][0] = ' ';
            m_Indent[i][1] = ' ';
            m_Indent[i][2] = ' ';
            m_Indent[i][3] = ' ';
            m_Indent[i][4] = '\0';
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
                    m_Log << "\n";
                    if (prefixPos0 != NULL) m_Log << prefixPos0;
                    if (prependIndent) m_Log << GetIndent();
                    if (prefix != NULL) m_Log << prefix;
                }
                else
                {
                    m_Log << *cptr;
                    cptr++;
                }
            }
        }

        void WriteToStream(const char* text, bool prependIndent, const char* prefixPos0, const char* prefix, bool appendNewline, bool flush, bool append)
        {
            if (!append)
            {
                if (prefixPos0 != NULL) m_Log << prefixPos0;
                if (prependIndent) m_Log << GetIndent();
                if (prefix != NULL) m_Log << prefix;
            }

            WriteToStream(text, prependIndent, prefixPos0, prefix);

            if (appendNewline) m_Log << '\n';
            if (flush) m_Log.flush();
        };

        void WriteToStream(const char* text, bool prependIndent, const char* prefixPos0, const char* prefix, bool appendNewline, bool flush)
        {
            WriteToStream(text, prependIndent, prefixPos0, prefix, appendNewline, flush, false);
        }

        void WriteToStream(const char* text, bool prependIndent, bool appendNewline, bool flush)
        {
            WriteToStream(text, prependIndent, NULL, NULL, appendNewline, flush);
        }

};
