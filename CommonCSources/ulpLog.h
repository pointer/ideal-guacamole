//  Copyright  UniCredit Services S.C.p.A.
//
//  FILE:      ulpLog.h
//
//  PURPOSE:   Header for a lightweight logger
//

#ifdef READER_PLUGIN
#include <PIHeaders.h>
#endif // READER_PLUGIN


#include <fstream>
#include <tchar.h>


#define TIME (char *)ulplog::INSERTTIME;

#define LPLOGNEWLINE() ulplog::LogLine("\n")
#define LPLOGLINE(text) ulplog::LogLine(text);
#define LPLOGLINENOIDENT(text) ulplog::LogLineNoIdent(text);
#define LPLOGLINEFLUSH(text) ulplog::LogLineFlush(text);
#define LPLOGERROR(text, e) ulplog::LogError(text, e);
#define LPLOGERRORWARNING(text) ulplog::LogErrorWarning(text);
#define LPLOGLASTERROR(text, bIndent, bErrorOnly) ulplog::LogLastErrorMessage(text, bIndent, bErrorOnly)
#define LPLOGENTERSECTION(text) ulplog::EnterSection(text);
#define LPLOGEXITSECTION(level) ulplog::ExitSection(level);
#define LPLOGVAR(name, value) ulplog::LogVar(name, value);
#define LPLOGVARL(name, value) ulplog::LogVarL(name, value);
#define LPLOGVARUL(name, value) ulplog::LogVarUL(name, value);
#define LPLOGADOBEERROR(text, error) ulplog::LogAdobeError(text, error);

namespace ulplog
{

    extern char* INSERTTIME;

    void InitLog(TCHAR* fileNamePart);
    void TerminateLog();

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
    void LogVar(const wchar_t * name, const char* value);
    void LogVar(const char * name, const char* value);
    void LogVarL(const char* name, signed __int64 value);
    void LogVarUL(const char* name, unsigned __int64 value);

    
#ifdef READER_PLUGIN

    void LogAdobeError(const char* text, ASInt32 error);

#endif
}