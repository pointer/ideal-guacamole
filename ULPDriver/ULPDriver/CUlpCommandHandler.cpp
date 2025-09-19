//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//  Copyright  1998 - 2003  Microsoft Corporation.  All Rights Reserved.
//
//  FILE:      command.cpp
//
//  PURPOSE:   Injects Postscript and writes via pipe to LPSpooler.exe
//
//  COMMENT:   Contains UniLogoPrint 2 specific logic 
//

#include "precomp.h"
#include "debug.h"
#include "resource.h"
#include "winspool.h"
#include <syncstream>
#include <future>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <crtdbg.h>  // For _CrtSetReportMode

#include <windows.h>
#include <tchar.h>
#include <psapi.h>

#include "CUlpLog.h"
#include "CUlpCommandHandler.h"
#include "ulpDriver.h"
#include "ulpHelper.h"
#include "CUlpSpoolerPipe.h"

#define sync_cout std::osyncstream(std::cout)

// This indicates to Prefast that this is a usermode driver file.
_Analysis_mode_(_Analysis_code_type_user_driver_);


// Fills char-buffer cbCurrentPageNumber with current page number
void CUlpCommandHandler::SetCurrentPageNumber(int n)
{
    _iCurrentPageNumber = n;
    ZeroMemory(_cbCurrentPageNumber, sizeof(_cbCurrentPageNumber));
    StringCbPrintfA(_cbCurrentPageNumber, sizeof(_cbCurrentPageNumber), "%d", n);
    _Log->LogVarUL("Current Page Number", n);
    _Log->LogFlush();
}

// Creates driver-job-id lDriverJobId and fills char-buffer cbDriverJobId 
void CUlpCommandHandler::CreateDriverJobId()
{
    ZeroMemory(_cbDriverJobId, sizeof(_cbDriverJobId));
    long lowerBoundDriverJobId = 10000000;
    long upperBoundDriverJobId = 99999999;
    srand((time(NULL) - 1538123990) & 0x8FFFFFFF);
    _lDriverJobId = ((rand() * RAND_MAX + rand()) % (upperBoundDriverJobId - lowerBoundDriverJobId)) + lowerBoundDriverJobId;
    StringCbPrintfA(_cbDriverJobId, sizeof(_cbDriverJobId), "%d", _lDriverJobId);
}

void CUlpCommandHandler::InitCommandHandler()
{

    _Log = new CUlpLog(_T("LogoPrint_LPDriver"));
    _Log->EnterSection("Initializing ULPCommandHandler (V1.20)");

    InitCommandNames();
    InitIsInjectCommand();

    ZeroMemory(LOGOPRINT_DSCCOMMAND, sizeof(LOGOPRINT_DSCCOMMAND));
    auto read_reg_str_future = std::async(&ulpHelper::ReadLogoPrintRegStr, HKEY_LOCAL_MACHINE, _T("DSCCommandCStylePattern"));
    ulpHelper::CharBuffer* pattern = read_reg_str_future.get();
    //ulpHelper::CharBuffer* pattern = ulpHelper::ReadLogoPrintRegStr(HKEY_LOCAL_MACHINE, _T("DSCCommandCStylePattern"));    // something like  %%UCSLogoPrint %s(%s) [%d]
    if (pattern != NULL) {
        strcpy_s(LOGOPRINT_DSCCOMMAND, sizeof(LOGOPRINT_DSCCOMMAND), pattern->GetBufferAnsi());
    }
    else {
        // Default if DSCCommandCStylePattern is not found
        strcpy_s(LOGOPRINT_DSCCOMMAND, sizeof(LOGOPRINT_DSCCOMMAND), "%%UCSLogoPrint %s(%s) [%s]");
    }

    size_t textLength = strnlen_s(LOGOPRINT_DSCCOMMAND, sizeof(LOGOPRINT_DSCCOMMAND));
    if (textLength < 0) textLength = 0;
    if (textLength <= sizeof(LOGOPRINT_DSCCOMMAND) - 3)
    {
        LOGOPRINT_DSCCOMMAND[textLength] = '\r';
        LOGOPRINT_DSCCOMMAND[textLength + 1] = '\n';
        LOGOPRINT_DSCCOMMAND[textLength + 2] = '\0';
    }
    else {
        LOGOPRINT_DSCCOMMAND[sizeof(LOGOPRINT_DSCCOMMAND) - 3] = '\r';
        LOGOPRINT_DSCCOMMAND[sizeof(LOGOPRINT_DSCCOMMAND) - 2] = '\n';
        LOGOPRINT_DSCCOMMAND[sizeof(LOGOPRINT_DSCCOMMAND) - 1] = '\0';
    }
    if (pattern)
    {
        pattern->free();
    }
    _Log->LogVar("LOGOPRINT_DSCCOMMAND", LOGOPRINT_DSCCOMMAND);

    ZeroMemory(SETPARAMIDCOMMANDNAME, sizeof(SETPARAMIDCOMMANDNAME));
    read_reg_str_future = std::async(&ulpHelper::ReadLogoPrintRegStr, HKEY_LOCAL_MACHINE, _T("DSCCommandSetParamId"));
    ulpHelper::CharBuffer* setParamIdDSCCommandName = read_reg_str_future.get();
    //ulpHelper::CharBuffer* setParamIdDSCCommandName = ulpHelper::ReadLogoPrintRegStr(HKEY_LOCAL_MACHINE, _T("DSCCommandSetParamId"));
    strcpy_s(SETPARAMIDCOMMANDNAME, sizeof(SETPARAMIDCOMMANDNAME), setParamIdDSCCommandName->GetBufferAnsi());
    _Log->LogVar("SETPARAMIDCOMMANDNAME", SETPARAMIDCOMMANDNAME);

    ZeroMemory(SETPARAMIDCOMMANDPREFIX, sizeof(SETPARAMIDCOMMANDPREFIX));
    read_reg_str_future = std::async(&ulpHelper::ReadLogoPrintRegStr, HKEY_LOCAL_MACHINE, _T("DSCCommandSetParamId"));
    ulpHelper::CharBuffer* DSC_Prefix = read_reg_str_future.get();
    //ulpHelper::CharBuffer* DSC_Prefix = ulpHelper::ReadLogoPrintRegStr(HKEY_LOCAL_MACHINE, _T("DSCPrefix"));
    sprintf_s(SETPARAMIDCOMMANDPREFIX, sizeof(SETPARAMIDCOMMANDPREFIX), "\r\n%s%s", DSC_Prefix->GetBufferAnsi(), setParamIdDSCCommandName->GetBufferAnsi());
    if (DSC_Prefix)
    {
        DSC_Prefix->free();
    }
    if (setParamIdDSCCommandName)
    {
        setParamIdDSCCommandName->free();
    }
    _Log->LogVar("SETPARAMIDCOMMANDPREFIX", SETPARAMIDCOMMANDPREFIX);

    // Initialize page number
    _Log->LogLine("Setting page number to 0 ...");
    SetCurrentPageNumber(0);

    //_bStreaming = true;

    // Create driver-job-id lDriverJobId and fill char-buffer cbDriverJobId 
    _Log->LogLine("Creating driverJobId ...");
    CreateDriverJobId();
    _Log->LogVar("cbDriverJobId", _cbDriverJobId);
    bool success = false;
    auto ReadLogoPrintRegInt = static_cast<DWORD(*)(HKEY, LPCTSTR, DWORD)>(ulpHelper::ReadLogoPrintRegInt);
    auto read_reg_int_future = std::async(ReadLogoPrintRegInt, HKEY_CURRENT_USER, _T("PSInjectToFail"),0);

    DWORD dwPSInjectToFail = read_reg_int_future.get();

    //DWORD dwPSInjectToFail = ulpHelper::ReadLogoPrintRegInt(HKEY_CURRENT_USER, _T("PSInjectToFail"), 0);

    if (dwPSInjectToFail > 0)
    {
        _dwPSInjectToFail = dwPSInjectToFail;
        read_reg_int_future = std::async( ReadLogoPrintRegInt, HKEY_CURRENT_USER, _T("PSInjectToFailErrorCode"), ERROR_BAD_PIPE);
        _dwPSInjectToFailErrorCode = read_reg_int_future.get();
        //_dwPSInjectToFailErrorCode = ulpHelper::ReadLogoPrintRegInt(HKEY_CURRENT_USER, _T("PSInjectToFailErrorCode"), ERROR_BAD_PIPE); // default ERROR_BAD_PIPE
    }

    _Log->LogLine("Creating driver debug file (if requested by reg) ...");
    CreateDriverPSDebugFile();

    _Log->LogLine("Starting LPSpooler and pipe ...");
    _UlpSpooler = new CUlpSpoolerPipe(_lDriverJobId, _Log);

    _Log->EnterSection("Start streaming postscript (V1.20)");
    _bIsInitalized = true;
}



// Opens a PostScript file to stream to, provided that a filename is specified in HKCU-LogoPrint2-Key DriverPSDebugFile
void CUlpCommandHandler::CreateDriverPSDebugFile()
{
    auto read_reg_str_future = std::async(&ulpHelper::ReadLogoPrintRegStr, HKEY_LOCAL_MACHINE, _T("LPDriverPSDebugFile"));
    ulpHelper::CharBuffer* driverPSFileName = read_reg_str_future.get();
    //ulpHelper::CharBuffer* driverPSFileName = ulpHelper::ReadLogoPrintRegStr(HKEY_CURRENT_USER, _T("LPDriverPSDebugFile"));
    if (driverPSFileName != NULL && driverPSFileName->Size() > 0)
    {
        char* filename = driverPSFileName->GetBufferAnsi();
        char fullname[MAX_PATH + 10];
        ZeroMemory(fullname, sizeof(fullname));
        sprintf_s(fullname, sizeof(fullname), "%s_%s.txt", filename, _cbDriverJobId);
        try
        {
            _streamPSDebugFile.open(fullname, std::ios::binary);
            _bWriteToPSDebugFile = _streamPSDebugFile.is_open();
        }
        catch (const std::exception & e)
        {
            _Log->LogError("Error opening ps debug file", e);
            _bWriteToPSDebugFile = false;
        }
        _Log->LogLineParts(const_cast<char*>("Will write PostScript to '"), filename, "' for debugging", NULL);
    }
    else 
    {
        _Log->LogLineParts(const_cast<char*>("No output of PostScript to a debug-file."), NULL);
    }

    if (driverPSFileName != NULL)
    {
        driverPSFileName->free();
    }
}

// Writes cBuffer to debug-file, if debug-file has been opened
void CUlpCommandHandler::WriteDriverDebugFile(const char* cBuffer, DWORD cbBuffer)
{
    if (_bWriteToPSDebugFile)
    {
        _streamPSDebugFile.write(cBuffer, cbBuffer);
    }
}

// Writes cBuffer to ULPSpooler using the established pipe
HRESULT CUlpCommandHandler::WriteToSpoolerPipe(const char* cBuffer, DWORD cbBuffer)
{
    HRESULT hr = S_OK;
    if (_dwWritePipeLastError == 0)
    {
        auto read_reg_str_future = std::async([this, cBuffer, cbBuffer]() {
            return _UlpSpooler->WriteToSpoolerPipe(cBuffer, cbBuffer, &_dwWritePipeLastError);
            });
        DWORD bytesWritten = read_reg_str_future.get();
        //DWORD bytesWritten = _UlpSpooler->WriteToSpoolerPipe(cBuffer, cbBuffer, &(_dwWritePipeLastError));
        if (_dwWritePipeLastError != 0)
        {
            if (_dwWritePipeLastError == 232)
            {
                _Log->LogLineFlush("Pipe has been closed by ULPSpooler -> Print is to be aborted!");
                _bCancel = true;
            }
            else
            {
                _Log->LogLineFlush("An error occured writing to the pipe -> Print is to be aborted!");
                _bErrorWritingPipe = true;
            }
            hr = ERROR_WRITE_FAULT;
        }

        if (bytesWritten != cbBuffer)
        {
            _Log->LogVarL("Bytes written", bytesWritten);
            _Log->LogVarL("Bytes to write", cbBuffer);
            _Log->LogLineFlush("Mismatch bytes written over pipe!");
        }
    }

    if (hr != S_OK)
    {
        _Log->LogVarL("hr", hr);
        _Log->LogLineFlush("Returning with error (see hr)!");
    }
    return hr;
}



   
// Redirects postscript received from system-spooler to ULPSpooler via pipe. 
// No postscript is sent to a printer/output, except to the MapId-File in some cases.
HRESULT CUlpCommandHandler::ULPWritePrinter(PDEVOBJ pdevobj, PVOID pBuf, DWORD cbBuffer, PDWORD pcbWritten)
{
    HRESULT hr;
    const char* cBuffer = static_cast<const char*>(pBuf);

    if (cBuffer != NULL && cbBuffer != 0)
    {
        //if (_bCheckWriteMapIdFile)
        //{
        //    CheckWriteMapIdFile(pdevobj, cBuffer);
        //}

        if (_bFirstWrite && !_bParameterIdHasValue) {
            DWORD cbWritten = 0;
            DWORD jobIdLen = (DWORD)strnlen_s(_cbDriverJobId, MAXSIZEDRIVERJOBID);
            if (jobIdLen > MAXSIZEDRIVERJOBID) jobIdLen = MAXSIZEDRIVERJOBID; // Fix wrong warning C6385
            if (jobIdLen > 0) {
                WritePrinter(pdevobj->hPrinter, _cbDriverJobId, jobIdLen, &cbWritten);
                WritePrinter(pdevobj->hPrinter, "\r\n", 2, &cbWritten);
                _Log->LogLineFlush("Wrote DriverJobId to LParam_*MapId.txt.");
            }
            _bFirstWrite = false; // Write driverJobId only once to MapId-file
        }

        DWORD cbBytesToStream = cbBuffer;

        WriteDriverDebugFile(cBuffer, cbBytesToStream);

        hr = WriteToSpoolerPipe(cBuffer, cbBytesToStream);

        *pcbWritten = cbBuffer;     // Make system-spooler believe that alle bytes are written 
                                    // (which they are, but to the pipe and not to the system-spooler)
    }
    else
    {
        *pcbWritten = cbBuffer;
        hr = S_OK;
    }

    return hr;
}

HRESULT CUlpCommandHandler::WriteToSysSpoolBuf(PDEVOBJ pdevobj, DWORD dwIndex, IPrintOemDriverPS* pOEMHelp, PDWORD pdwReturn, PSTR pProcedure)
{
    HRESULT hResult = E_FAIL;
    DWORD   dwLen = 0;
    DWORD   dwSize = 0;

    if (_bCancel || _bErrorWritingPipe)
    {
        _Log->LogLineFlush("Print is aborted!");
        // Flags indicate to not inject postscripts any more
        *pdwReturn = _dwWritePipeLastError;
        return E_FAIL;
    }

    // UniLogoPrint does not support replacement of postscript created by the core driver
    bool isReplaceOfPScript5DriversPostscript = !_cIsInjectCommand[dwIndex]; // Prevent PScript5-driver's postscript to be replaced by this plugin
    if (isReplaceOfPScript5DriversPostscript) {
        if (NULL != pProcedure)
        {
            _Log->LogLineFlush("!!! Plugin-postscript has not been written to system-spooler's buffer because PScript5-driver's postscript must not be replaced!");
        }
        *pdwReturn = ERROR_NOT_SUPPORTED;
        return  E_NOTIMPL;
    }

    if (pProcedure == NULL)
    {
        // No special postscript-to-inject for this injection point has been defined 
        // -> inject proprietary default DSC comment to mark injection point for ULPSpooler
        pProcedure = MakeLogoPrintPSInjectCommand(dwIndex);
        if (pProcedure == NULL)
        {
            // SHould not happen
            *pdwReturn = ERROR_NOT_SUPPORTED;
            return  E_NOTIMPL;
        }
    }

    // A DSC comment (=pProcedure) is to be injected -> write content of pProcedure to spool buffer
    dwLen = (DWORD)strnlen(pProcedure, MAXPADCHARS);
    if (dwLen > 0 && dwLen < MAXPADCHARS)
    {
        SetLastError(S_OK);
        hResult = pOEMHelp->DrvWriteSpoolBuf(pdevobj, pProcedure, dwLen, &dwSize);
        if (dwLen != dwSize)
        {
            _Log->LogVarUL("Bytes to send", dwLen);
            _Log->LogVarUL("Bytes sent", dwSize);
            _Log->LogLineFlush("Did not succeed to write all bytes to system-spooler's buffer!");
        }
    }
    else
    {
        dwLen = 0;
        _Log->LogLineFlush("No bytes to write to system-spooler's buffer!");
    }

    // Set return values.
    if (dwLen == 0 || (SUCCEEDED(hResult) && (dwLen == dwSize)))
    {
        *pdwReturn = ERROR_SUCCESS;
    }
    else
    {
        // Try to return meaningful
        // error value.
        *pdwReturn = GetLastError();
        _Log->LogVarL("LastError from DrvWriteSpoolBuf", *pdwReturn);
        _Log->LogVarL("hResult from DrvWriteSpoolBuf", hResult);
        if (ERROR_SUCCESS == *pdwReturn)
        {
            *pdwReturn = ERROR_WRITE_FAULT;
        }

        // Make sure we return failure
        // if the write didn't succeded.
        if (SUCCEEDED(hResult))
        {
            _Log->LogVarL("hResult from DrvWriteSpoolBuf", hResult);
            hResult = HRESULT_FROM_WIN32(*pdwReturn);
        }
        _Log->LogVarL("ErrorCode returned", *pdwReturn);
        _Log->LogVarL("hResult returned", hResult);
        _Log->LogLineFlush("Did not succeed to write system-spooler's buffer!");
    }

    // dwLen should always equal dwSize.
    ASSERT(dwLen == dwSize);

    return hResult;
}


HRESULT CUlpCommandHandler::ULPCommandInject(PDEVOBJ pdevobj, DWORD dwIndex, PVOID pData, DWORD cbSize,
                                                IPrintOemDriverPS* pOEMHelp, PDWORD pdwReturn)
{
    PSTR    pProcedure = NULL;
    HRESULT hResult = E_FAIL;
    // UniLogoPrint does not support injection of postscript before PSINJECT_PSADOBE or after PSINJECT_EOF
    bool bAllowInjectPostscript = _bHaveSeenPSAdobe && !_bHaveSeenEOF; // Inject PostScript after(!) PSINJECT_PSADOBE and not after PSINJECT_EOF


    VERBOSE(DLLTEXT("Entering OEMCommand...\r\n"));

    UNREFERENCED_PARAMETER(pData);
    UNREFERENCED_PARAMETER(cbSize);

    if (!_bIsInitalized) {
        InitCommandHandler();
    }

    int level = -1;

    if (dwIndex > 0 && dwIndex <= MAXCOMMAND && _cCommandName[dwIndex] != NULL) {
        level = _Log->EnterSection(_cCommandName[dwIndex]);
    }

    switch (dwIndex)
    {
        case PSINJECT_BEGINPAGESETUP:
            _Log->LogLineFlush("-> will increment current page number ...");
            SetCurrentPageNumber(_iCurrentPageNumber + 1);
            break;

        case PSINJECT_BEGINSTREAM:
            break;

        case PSINJECT_PSADOBE:
            _bHaveSeenPSAdobe = true;
            break;

        case PSINJECT_COMMENTS:
            if (_bParameterIdHasValue) {
                _Log->LogLineFlush("Creating SetParameterId command ...");
                pProcedure = MakeLogoPrintDSCCommand(SETPARAMIDCOMMANDNAME, _cParameterId); // PostScript to inject something like:  %UCSLogoPrint SetParameterId(1252400638494244396315693) [81906903]
                _Log->LogVar("SetParameterId command", pProcedure);
            }
            break;
        case PSINJECT_ENDSTREAM:
            break;
        case PSINJECT_EOF: 
            _bHaveSeenEOF = true;
            break;
        default:
            VERBOSE(DLLTEXT("PSCommand Default...\r\n"));
            break;
    }

    if (_dwPSInjectToFail == dwIndex)
    {
        // Send hResult = E_FAIL for testing 
        hResult = E_FAIL;
        *pdwReturn = _dwPSInjectToFailErrorCode;
        _Log->LogVarL("ErrorCode returned (testing)", *pdwReturn);
        _Log->LogVarL("hResult returned (testing)", hResult);
        _Log->LogLine("Will send hResult=E_Fail for testing!");
    }
    else
    {
        if (bAllowInjectPostscript)
        {
            hResult = WriteToSysSpoolBuf(pdevobj, dwIndex, pOEMHelp, pdwReturn, pProcedure);
        }
        else
        {
            _Log->LogLineFlush("Will not inject postscript before PSINJECT_PSADOBE and after PSINJECT_EOF!");
            *pdwReturn = ERROR_SUCCESS;
            hResult = S_OK;
        }
    }

    if (level > -1) {
        _Log->ExitSection(level);
    }
    _Log->LogFlush();

    return hResult;

}

