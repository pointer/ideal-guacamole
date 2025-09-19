//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//  Copyright  1997 - 2003  Microsoft Corporation.  All Rights Reserved.
//
//  FROM:      WDK - OEM Printer Customization Plug-in Samples
//
//  FILE:      ulpCommand.h
//
//  PURPOSE:   Injects Postscript and writes via pipe to ULPSpooler.exe
//
//  COMMENT:   Modification by UniLogoPrint 2
//
#pragma once

#include "intrface.h"
#include <iostream>
#include <fstream>
#include "CUlpLog.h"
#include "ulpHelperUsingLog.h"
#include "CUlpSpoolerPipe.h"

const DWORD MAXSIZEDRIVERJOBID = 20;  //buffer size large enough for driver's jobId
const DWORD MAXSIZEPAGENUMBER = 10;   //buffer size large enough for pagenumber
const DWORD PLACEHOLDERMAXSIZE = 255; //buffer size large enough to hold any used placeholder
const DWORD MAXPADCHARS = 8192;       //maximum number of padding chars to test overlapping LOGOPRINT_EOF_DSCCOMMENT
const int MAXCOMMAND = 201;
const DWORD DSCCOMMANDMAXSIZE = 150;   //buffer size large enough to hold command-pattern
const DWORD SETPARAMIDCOMMANDNAMESIZE = 30;   //buffer size large enough to SetParamId-command


class CUlpCommandHandler
{

private:

    // Fills char-buffer cbCurrentPageNumber with current page number
    void SetCurrentPageNumber(int n);

    // Creates driver-job-id lDriverJobId and fills char-buffer cbDriverJobId 
    void CreateDriverJobId();

    // Returns pointer to char-buffer containing proprietary DSC comment for injection point with name cName
    char* MakeLogoPrintDSCCommand(const char* cName, char* paramValue)
    {
        char* result = NULL;
        if (cName != NULL)
        {
            _Log->LogLineParts(const_cast<char*>("Inserting mark for '"), cName, "' (page# ", _cbCurrentPageNumber, ")", NULL);
            ZeroMemory(_bufferPSToInject, sizeof(_bufferPSToInject));
            if (SUCCEEDED(StringCbPrintfA(_bufferPSToInject, sizeof(_bufferPSToInject), LOGOPRINT_DSCCOMMAND, cName, paramValue, _cbDriverJobId)))
            {
                result = _bufferPSToInject;
            }
        }
        return result;
    }

    // Returns pointer to char-buffer containing proprietary DSC comment for injection point with cCommandName specified by dwIndex
    char* MakeLogoPrintPSInjectCommand(DWORD dwIndex)
    {
        char* result = NULL;

        if (dwIndex > 0 && dwIndex <= MAXCOMMAND && _cCommandName[dwIndex] != NULL)
        {
            result = MakeLogoPrintDSCCommand(_cCommandName[dwIndex], _cbCurrentPageNumber);
        }
        return result;
    }

    // Opens a debug-file to log to, provided that a filename is specified in HKCU-LogoPrint2-Key DriverPSDebugFile
    void CreateDriverPSDebugFile();

    // Writes cBuffer to debug-file, if debug-file has been opened
    void WriteDriverDebugFile(const char* cBuffer, DWORD cbBuffer);

    // Writes cBuffer to ULPSpooler using the established pipe
    HRESULT WriteToSpoolerPipe(const char* cBuffer, DWORD cbBuffer);

    HRESULT WriteToSysSpoolBuf(PDEVOBJ pdevobj, DWORD dwIndex, IPrintOemDriverPS* pOEMHelp, PDWORD pdwReturn, PSTR pProcedure);

    void InitCommandHandler();

    HRESULT WriteToSpoolerPipe(char* cBuffer, DWORD cbBuffer);


public:

    __stdcall CUlpCommandHandler(PWSTR pPrinterName)
    {
        VERBOSE("In CUlpCommandHandler constructor...");
        _bIsInitalized = false;
        _bCancel = false;
        _bErrorWritingPipe = false;
        _dwWritePipeLastError = 0;
        _bWriteToPSDebugFile = false;
        _bHaveSeenPSAdobe = false;
        _bHaveSeenEOF = false;
        _bFirstWrite = true;
        _bParameterIdHasValue = false;
        _bWriteToPSDebugFile = false;
        _lDriverJobId = 0;
        _iCurrentPageNumber = 0;
        _dwPSInjectToFail = 0;
        _dwPSInjectToFailErrorCode = 0;
        
        ZeroMemory(_cParameterId, sizeof(_cParameterId));
        char printerNameAnsi[MAX_PATH + 10];
        ZeroMemory(printerNameAnsi, sizeof(printerNameAnsi));
        size_t printerNameLength = wcsnlen(pPrinterName, MAX_PATH + 10);
        if (printerNameLength > 0 && printerNameLength < MAX_PATH + 10)
        {
            // printerName can be 'UniLogoPrint 2' or a MapId-file which ends in '.txt, Port'
            size_t charsConverted;
            wcstombs_s(&charsConverted, printerNameAnsi, MAX_PATH + 10, pPrinterName, printerNameLength);
            int i = (int)printerNameLength - 4;
            while (i >= 0 && (printerNameAnsi[i] < '0' || printerNameAnsi[i] > '9')) i--; // find the end of the parameter-id in pn
            int afterEndOfParameterId = i + 1;  // the character after(!) the last digit of parameter-id
            while (i >= 0 && printerNameAnsi[i]>='0' && printerNameAnsi[i]<='9') i--;   // find the start of the parameter-id in pn
            int startPosOfParameterId = i + 1; // Last index where character 0-9 was found
            int lenOfParameterId = afterEndOfParameterId - startPosOfParameterId;
            if (startPosOfParameterId >= 0 && lenOfParameterId > 10 && lenOfParameterId < sizeof(_cParameterId))
            {
                // Parameter-id (at least of length 2) was found -> store in _cParameterId
                for (i = 0; i < lenOfParameterId; i++) {
                    _cParameterId[i] = printerNameAnsi[startPosOfParameterId + i];
                }
                _bParameterIdHasValue = true;
            }
        }

        _Log = NULL;
        _UlpSpooler = NULL;

        ZeroMemory(_cbDriverJobId, sizeof(_cbDriverJobId));
        ZeroMemory(_cbCurrentPageNumber, sizeof(_cbCurrentPageNumber));
        ZeroMemory(_bufferPSToInject, sizeof(_bufferPSToInject));

    }

    __stdcall ~CUlpCommandHandler(void)
    {
        VERBOSE("In CUlpCommandHandler destructor ...");

        if (_Log != NULL) _Log->LogLine("CUlpCommandHandler destructor ...");
        
        if (_bWriteToPSDebugFile)
        {
            if (_Log != NULL) _Log->LogLine("Flushing and closing PostScript debug file ...");
            _bWriteToPSDebugFile = false;
            _streamPSDebugFile.flush();
            _streamPSDebugFile.close();
        }

        if (_Log != NULL) _Log->LogLine("Closing LPSpooler and pipe ...");
        delete _UlpSpooler;

        if (_Log != NULL) _Log->ExitSection(0);
        delete _Log;

    }


    HRESULT ULPCommandInject(PDEVOBJ pdevobj, DWORD dwIndex, PVOID pData, DWORD cbSize, IPrintOemDriverPS* pOEMHelp, PDWORD pdwReturn);

    HRESULT ULPWritePrinter(PDEVOBJ, PVOID, DWORD, PDWORD);


private:

    void InitCommandNames()
    {
        _Log->LogLine("Initializing command names ...");

        // Init command names
        ZeroMemory(_cCommandName, sizeof(_cCommandName));
        _cCommandName[PSINJECT_BEGINSTREAM] = "PSINJECT_BEGINSTREAM";
        _cCommandName[PSINJECT_PSADOBE] = "PSINJECT_PSADOBE";
        _cCommandName[PSINJECT_PAGESATEND] = "PSINJECT_PAGESATEND";
        _cCommandName[PSINJECT_PAGES] = "PSINJECT_PAGES";
        _cCommandName[PSINJECT_DOCNEEDEDRES] = "PSINJECT_DOCNEEDEDRES";
        _cCommandName[PSINJECT_DOCSUPPLIEDRES] = "PSINJECT_DOCSUPPLIEDRES";
        _cCommandName[PSINJECT_PAGEORDER] = "PSINJECT_PAGEORDER";
        _cCommandName[PSINJECT_ORIENTATION] = "PSINJECT_ORIENTATION";
        _cCommandName[PSINJECT_BOUNDINGBOX] = "PSINJECT_BOUNDINGBOX";
        _cCommandName[PSINJECT_DOCUMENTPROCESSCOLORS] = "PSINJECT_DOCUMENTPROCESSCOLORS";
        _cCommandName[PSINJECT_COMMENTS] = "PSINJECT_COMMENTS";
        _cCommandName[PSINJECT_BEGINDEFAULTS] = "PSINJECT_BEGINDEFAULTS";
        _cCommandName[PSINJECT_ENDDEFAULTS] = "PSINJECT_ENDDEFAULTS";
        _cCommandName[PSINJECT_BEGINPROLOG] = "PSINJECT_BEGINPROLOG";
        _cCommandName[PSINJECT_ENDPROLOG] = "PSINJECT_ENDPROLOG";
        _cCommandName[PSINJECT_BEGINSETUP] = "PSINJECT_BEGINSETUP";
        _cCommandName[PSINJECT_ENDSETUP] = "PSINJECT_ENDSETUP";
        _cCommandName[PSINJECT_TRAILER] = "PSINJECT_TRAILER";
        _cCommandName[PSINJECT_EOF] = "PSINJECT_EOF";
        _cCommandName[PSINJECT_ENDSTREAM] = "PSINJECT_ENDSTREAM";
        _cCommandName[PSINJECT_DOCUMENTPROCESSCOLORSATEND] = "PSINJECT_DOCUMENTPROCESSCOLORSATEND";
        _cCommandName[PSINJECT_PAGENUMBER] = "PSINJECT_PAGENUMBER";
        _cCommandName[PSINJECT_BEGINPAGESETUP] = "PSINJECT_BEGINPAGESETUP";
        _cCommandName[PSINJECT_ENDPAGESETUP] = "PSINJECT_ENDPAGESETUP";
        _cCommandName[PSINJECT_PAGETRAILER] = "PSINJECT_PAGETRAILER";
        _cCommandName[PSINJECT_PLATECOLOR] = "PSINJECT_PLATECOLOR";
        _cCommandName[PSINJECT_SHOWPAGE] = "PSINJECT_SHOWPAGE";
        _cCommandName[PSINJECT_PAGEBBOX] = "PSINJECT_PAGEBBOX";
        _cCommandName[PSINJECT_ENDPAGECOMMENTS] = "PSINJECT_ENDPAGECOMMENTS";
        _cCommandName[PSINJECT_VMSAVE] = "PSINJECT_VMSAVE";
        _cCommandName[PSINJECT_VMRESTORE] = "PSINJECT_VMRESTORE";
    }

    void InitIsInjectCommand()
    {
        _Log->LogLine("Initializing IsInjectCommand array ...");

        _cIsInjectCommand[PSINJECT_BEGINSTREAM] = true;
        _cIsInjectCommand[PSINJECT_PSADOBE] = true;
        _cIsInjectCommand[PSINJECT_PAGESATEND] = false; // Will not inject but replace!
        _cIsInjectCommand[PSINJECT_PAGES] = false; // Will not inject but replace!
        _cIsInjectCommand[PSINJECT_DOCNEEDEDRES] = true;
        _cIsInjectCommand[PSINJECT_DOCSUPPLIEDRES] = true;
        _cIsInjectCommand[PSINJECT_PAGEORDER] = false; // Will not inject but replace!
        _cIsInjectCommand[PSINJECT_ORIENTATION] = false; // Will not inject but replace!
        _cIsInjectCommand[PSINJECT_BOUNDINGBOX] = false; // Will not inject but replace!
        _cIsInjectCommand[PSINJECT_DOCUMENTPROCESSCOLORS] = false; // Will not inject but replace!
        _cIsInjectCommand[PSINJECT_COMMENTS] = true;
        _cIsInjectCommand[PSINJECT_BEGINDEFAULTS] = true;
        _cIsInjectCommand[PSINJECT_ENDDEFAULTS] = true;
        _cIsInjectCommand[PSINJECT_BEGINPROLOG] = true;
        _cIsInjectCommand[PSINJECT_ENDPROLOG] = true;
        _cIsInjectCommand[PSINJECT_BEGINSETUP] = true;
        _cIsInjectCommand[PSINJECT_ENDSETUP] = true;
        _cIsInjectCommand[PSINJECT_TRAILER] = true;
        _cIsInjectCommand[PSINJECT_EOF] = true;
        _cIsInjectCommand[PSINJECT_ENDSTREAM] = true;
        _cIsInjectCommand[PSINJECT_DOCUMENTPROCESSCOLORSATEND] = false; // Will not inject but replace!
        _cIsInjectCommand[PSINJECT_PAGENUMBER] = false; // Will not inject but replace! 
        _cIsInjectCommand[PSINJECT_BEGINPAGESETUP] = true;
        _cIsInjectCommand[PSINJECT_ENDPAGESETUP] = true;
        _cIsInjectCommand[PSINJECT_PAGETRAILER] = true;
        _cIsInjectCommand[PSINJECT_PLATECOLOR] = false; // Will not inject but replace!
        _cIsInjectCommand[PSINJECT_SHOWPAGE] = false; // Will not inject but replace!
        _cIsInjectCommand[PSINJECT_PAGEBBOX] = false; // Will not inject but replace!
        _cIsInjectCommand[PSINJECT_ENDPAGECOMMENTS] = true;
        _cIsInjectCommand[PSINJECT_VMSAVE] = true;
        _cIsInjectCommand[PSINJECT_VMRESTORE] = true;
    }

    CUlpSpoolerPipe* _UlpSpooler;

    // Logger
    CUlpLog* _Log;

    bool _bParameterIdHasValue;
    // Parameter-id (derived from the MapId-file in case of print-to-file print-job, typically in MS Word)
    char _cParameterId[MAX_PATH + 10];

    // Array of propietary DSC comments used to mark injection points for evaluation by ULPSpooler
    const char* _cCommandName[MAXCOMMAND + 1];
    bool _cIsInjectCommand[MAXCOMMAND + 1];

    // Propietary DSC pattern used for poastscript injections (see HKLM-LogoPrint2-Key LogoPrintDSCCommandCStylePattern)
    CHAR LOGOPRINT_DSCCOMMAND[DSCCOMMANDMAXSIZE];

    // How the propietary DSC comment for SetParamId-command starts (is used to identify a SetParamId-command)
    CHAR SETPARAMIDCOMMANDPREFIX[PLACEHOLDERMAXSIZE];

    // Propietary DSC comment for SetParamId-command 
    CHAR SETPARAMIDCOMMANDNAME[SETPARAMIDCOMMANDNAMESIZE];

    // Flag indicating whether this class is initialized 
    bool  _bIsInitalized;

    // Flag indicating if ULPSpooler.exe closed pipe to abort spooling
    bool  _bCancel;

    // Flag indicating if there has been an error writeing to the pipe 
    bool  _bErrorWritingPipe;

    // LastError when writing to pipe 
    DWORD _dwWritePipeLastError;

    // Flag indicating if postscript injection point PSINJECT_PSADOBE has been reached
    // Used to not inject Postscript before this injection point!
    bool _bHaveSeenPSAdobe;

    // Flag indicating if postscript injection point PSINJECT_EOF has been reached
    // Used to not inject Postscript after this injection point!
    bool _bHaveSeenEOF;

    // Flag indicating if it's the first time that WritePrinter is called
    bool _bFirstWrite;

    // Received postscript sent by system-spooler will be written/logged to this ofstream
    std::ofstream _streamPSDebugFile;

    // Flag indicating if postscript sent by system-spooler has to be logged to streamPSDebugFile
    bool _bWriteToPSDebugFile;

    // DriverJob-Id used:
    // + in injected postscript as marker
    // + will be written to output (MapId-file), when printing application does not send a SetParamId-command
    //   and allows to identify the parameter-file to be used by LPSpooler.exe
    // + will be passed to LPSpooler.exe to identify the parameter-file when SetParamId-command is not sent 
    //   by printing application
    long _lDriverJobId;
    CHAR _cbDriverJobId[MAXSIZEDRIVERJOBID];

    // Current page number (used as parameter in injected postscript)
    int  _iCurrentPageNumber;
    CHAR _cbCurrentPageNumber[MAXSIZEPAGENUMBER];

    // Buffer for Propietary DSC postscript to inject
    CHAR _bufferPSToInject[PLACEHOLDERMAXSIZE];

    //PSInjectCommand that has to fail and send hResult=E_FAIL (for testing)
    DWORD _dwPSInjectToFail;

    //Error number returned to system when PSInjectCommand fails (for testing)
    DWORD _dwPSInjectToFailErrorCode;

};
typedef CUlpCommandHandler* PULPCOMMANDHANDLER;