//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//  Copyright  1998 - 2003  Microsoft Corporation.  All Rights Reserved.
//
//  FILE:      command.cpp
//
//  PURPOSE:   Injects Postscript and writes via pipe to ULPSpooler.exe
//
//  COMMENT:   Contains UniLogoPrint 2 specific logic 
//

#include "precomp.h"
#include "debug.h"
#include "resource.h"
#include "winspool.h"

#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <crtdbg.h>  // For _CrtSetReportMode

#include <windows.h>
#include <tchar.h>
#include <psapi.h>

#include "COemPDEV.h"
#include "CUlpState.h"
#include "ulpCommand.h"
#include "ulpDriverPS.h"
#include "ulpHelper.h"
#include "ulpLog.h"
#include "ulpSpoolerPipe.h"


// This indicates to Prefast that this is a usermode driver file.
_Analysis_mode_(_Analysis_code_type_user_driver_);

namespace ulpcommand
{


    /////////////////////////////////////////////////////////
    //      Internal Macros & Defines
    /////////////////////////////////////////////////////////



    /////////////////////////////////////////////////////////
    //      Internal ProtoTypes
    /////////////////////////////////////////////////////////


    void PreprocessKnuthMorrisPratt(char* x, long m, int kmpNext[])
    {
        // See Knuth-Morris-Pratt algorithm 
        // C implementation by Christian Charras and Thierry Lecroq
        // https://www-igm.univ-mlv.fr/~lecroq/string/node8.html
        int i, j;

        i = 0;
        j = kmpNext[0] = -1;
        while (i < m) {
            while (j > -1 && x[i] != x[j])
                j = kmpNext[j];
            i++;
            j++;
            if (x[i] == x[j])
                kmpNext[i] = kmpNext[j];
            else
                kmpNext[i] = j;
        }
    }



    // Returns pointer to char-buffer containing proprietary DSC comment for injection point with name cName
    CHAR* MakeLogoPrintDSCCommand(PULPSTATE me, const char* cName, char* paramValue)
    {
        CHAR* result = NULL;
        if (cName != NULL)
        {
            ulplog::LogLineParts("Inserting mark for '", cName, "' (page# ", me->_cbCurrentPageNumber, ")", NULL);
            ZeroMemory(me->_bufferPSToInject, sizeof(me->_bufferPSToInject));
            if (SUCCEEDED(StringCbPrintfA(me->_bufferPSToInject, sizeof(me->_bufferPSToInject), LOGOPRINT_DSCCOMMAND, cName, paramValue, me->_cbDriverJobId)))
            {
                result = me->_bufferPSToInject;
            }
        }
        return result;
    }

    // Returns pointer to char-buffer containing proprietary DSC comment for injection point with cCommandName specified by dwIndex
    CHAR* MakeLogoPrintPSInjectCommand(PULPSTATE me, DWORD dwIndex)
    {
        CHAR* result = NULL;

        if (dwIndex > 0 && dwIndex <= MAXCOMMAND && me->cCommandName[dwIndex] != NULL)
        {
            result = MakeLogoPrintDSCCommand(me, me->cCommandName[dwIndex], me->_cbCurrentPageNumber);
        }
        return result;
    }

    // Writes cBuffer to debug-file, if debug-file has been opened
    void WriteDriverDebugFile(PULPSTATE me, const char* cBuffer, DWORD cbBuffer)
    {
        if (me->_bWriteToPSDebugFile)
        {
            me->_streamPSDebugFile.write(cBuffer, cbBuffer);
        }
    }

    // Writes cBuffer to ULPSpooler using the established pipe
    HRESULT WriteToSpoolerPipe(PULPSTATE me, const char* cBuffer, DWORD cbBuffer)
    {
        ulplog::LogLineFlush("WriteToSpoolerPipe ...");

        HRESULT hr = S_OK;
        if (me->_dwWritePipeLastError == 0)
        {
            DWORD bytesWritten = ulppipe2spooler::WriteToSpoolerPipe(cBuffer, cbBuffer, &(me->_dwWritePipeLastError));
            if (me->_dwWritePipeLastError != 0)
            {
                if (me->_dwWritePipeLastError == 232)
                {
                    ulplog::LogLineFlush("Pipe has been closed by ULPSpooler -> Print is to be aborted!");
                    me->_bCancel = true;
                }
                else
                {
                    ulplog::LogLineFlush("An error occured writing to the pipe -> Print is to be aborted!");
                    me->_bErrorWritingPipe = true;
                }
                hr = ERROR_WRITE_FAULT;
            }

            if (bytesWritten != cbBuffer)
            {
                ulplog::LogVarL("Bytes written", bytesWritten);
                ulplog::LogVarL("Bytes to write", cbBuffer);
                ulplog::LogLineFlush("Mismatch bytes written over pipe!");
            }
        }

        if (hr != S_OK)
        {
            ulplog::LogVarL("hr", hr);
            ulplog::LogLineFlush("Returning with error (see hr)!");
        }
        ulplog::LogLineFlush("End of WriteToSpoolerPipe");
        return hr;
    }

    // Stop streaming, which means that pipe, debug- and log-file are closed and resources freed 
    void StopStreamingOnEndOfStream(PULPSTATE me)
    {
        if (me->_bWriteToPSDebugFile)
        {
            me->_bWriteToPSDebugFile = false;
            me->_streamPSDebugFile.flush();
            me->_streamPSDebugFile.close();
        }
        ulppipe2spooler::CleanSpoolerResources();
        LPLOGEXITSECTION(0);
        ulplog::TerminateLog();
    }

    //Checks wether 
    void CheckWriteMapIdFile(PULPSTATE me, PDEVOBJ pdevobj, const char* cBuffer)
    {
        if (me->_bHaveSeenEndComments)
        {
            // It's expected to find the SetParamId-command now
            // Either the SetParamId-command has been injected by the printing process (Adobe PlugIn)
            // or it's missing (Microsoft Word COMAddIn)
            if (me->_bCheckWriteMapIdFile)
            {
                ulplog::LogLineFlush("Checking out-buffer for SetParameterId-command ...");
                const char* pch = strstr(cBuffer, SETPARAMIDCOMMANDPREFIX);
                boolean bfoundSetParameterIdCommandWithId = false;
                if (pch != NULL)
                {
                    pch += strnlen(SETPARAMIDCOMMANDPREFIX, sizeof(SETPARAMIDCOMMANDPREFIX)) + 1;
                    bfoundSetParameterIdCommandWithId = (*pch) != ')';
                }

                if (bfoundSetParameterIdCommandWithId)
                {
                    // Found SetParamId-command -> nothing to do
                    // This case handles printing by Adobe PlugIn
                    ulplog::LogLineFlush("Printing application has sent SetParameterId-command.");
                }
                else
                {
                    //Did not find SetParamId-command
                    //Printing application has not sent SetParameterId-command and is expcted to print to a file which is named like the parameter file.
                    //To allow identification write DriverJobId to file-to-print-to
                    // This case handles printing by Microsoft Word COMAddIn
                    ulplog::LogLineFlush("Printing application has not sent SetParameterId-command and is expected to have a launched a print-to-file print-job!");
                    DWORD cbWritten = 0;
                    WritePrinter(pdevobj->hPrinter, me->_cbDriverJobId, (DWORD)strnlen(me->_cbDriverJobId, MAXSIZEDRIVERJOBID), &cbWritten);
                    WritePrinter(pdevobj->hPrinter, "\r\n", 2, &cbWritten);
                }
            }
            me->_bCheckWriteMapIdFile = false; // Check only once
        }
    }

    bool IsLogoPrintEOFFound(PULPSTATE me, const char* spoolerBuffer, DWORD cbSpoolerBuffer, DWORD* cbBytesToStream)
    {
        bool bStopStreaming = false;
        *cbBytesToStream = cbSpoolerBuffer;

        // See Knuth-Morris-Pratt algorithm 
        // C implementation by Christian Charras and Thierry Lecroq
        // https://www-igm.univ-mlv.fr/~lecroq/string/node8.html
        
        /* Searching */
        int i;
        DWORD j;

        const char* x = me->_LOGOPRINT_EOF_DSCCOMMENT;
        int m = me->_dwCommentLength;
        const char* y = spoolerBuffer;
        DWORD n = cbSpoolerBuffer;
        
        i = me->_dwCharPosComment;
        j = 0;
        while (j < n) {
            while (i > -1 && x[i] != y[j])
            {
                i = me->_kmpNext[i];
            }
            i++;
            j++;
            if (i >= m) {
                *cbBytesToStream = j;
                bStopStreaming = true;
                i = me->_kmpNext[i];
                break;
            }
        }
        me->_dwCharPosComment = i;
        return bStopStreaming;
    }

   
    // Redirects postscript received from system-spooler to ULPSpooler via pipe. 
    // No postscript is sent to a printer/output, except to the MapId-File in some cases.
    HRESULT ULPWritePrinter(PULPSTATE me, PDEVOBJ pdevobj, PVOID pBuf, DWORD cbBuffer, PDWORD pcbWritten)
    {
        HRESULT hr;
        const char* cBuffer = static_cast<const char*>(pBuf);

        if (cBuffer != NULL && cbBuffer != 0)
        {
            if (me->_bCheckWriteMapIdFile)
            {
                CheckWriteMapIdFile(me, pdevobj, cBuffer);
            }

            bool bStopStreaming = false;
            DWORD cbBytesToStream = cbBuffer;
            if (!me->_bStreaming)
            {
                // Check whether streaming is to be stopped now! There may be multiple calls of ULPWritePrinter till end of stream is reached.
                ulplog::LogLineFlush("Check whether streaming is to be stopped now ...");

                // Search buffer for _LOGOPRINT_EOF_DSCCOMMENT (or the beginning of _LOGOPRINT_EOF_DSCCOMMENT in case of rest will be contained in next buffer)
                CHAR partFound[PLACEHOLDERMAXSIZE];
                ZeroMemory(partFound, sizeof(partFound));
                bStopStreaming = IsLogoPrintEOFFound(me, cBuffer, cbBuffer, &cbBytesToStream);
                ulplog::LogVarUL("BufferSize", cbBuffer);
                if (bStopStreaming)
                {
                    ulplog::LogLineFlush("Found injected eofstream-comment (or its rest) -> Will stop streaming now!");
                    if (cbBytesToStream < me->_dwCommentLength)
                    {
                        strncpy_s(partFound, cBuffer, cbBytesToStream);
                    }
                    else
                    {
                        strncpy_s(partFound, cBuffer + (cbBuffer - me->_dwCommentLength), me->_dwCommentLength);
                    }
                    ulplog::LogVar("_LOGOPRINT_EOF_DSCCOMMENT-part found", partFound);
                }
                else
                {
                    if (me->_dwCharPosComment > 0)
                    {
                        ulplog::LogLineFlush("Buffer does not contain complete eofstream-comment (but - maybe - ends with a part of it) -> Will not stop streaming now!");
                        if (me->_dwCharPosComment < sizeof(partFound))
                        {
                            strncpy_s(partFound, me->_LOGOPRINT_EOF_DSCCOMMENT, me->_dwCharPosComment);
                            ulplog::LogVar("_LOGOPRINT_EOF_DSCCOMMENT-part found", partFound);

                        }
                        else
                        {
                            ulplog::LogLineFlush("Unexpected size of matching _LOGOPRINT_EOF_DSCCOMMENT-part!!!");
                        }
                    }
                    else
                    {
                        ulplog::LogLineFlush("Buffer does not contain eofstream-comment -> Will not stop streaming now!");
                    }

                }
            }

            WriteDriverDebugFile(me, cBuffer, cbBytesToStream);

            hr = WriteToSpoolerPipe(me, cBuffer, cbBytesToStream);

            *pcbWritten = cbBuffer;     // Make system-spooler believe that alle bytes are written 
                                        // (which they are but to the pipe and not to the system-spooler)

            if (bStopStreaming)
            {
                StopStreamingOnEndOfStream(me);
            }
        }
        else
        {
            *pcbWritten = cbBuffer;
            hr = S_OK;
        }

        return hr;
    }

    HRESULT WritetoSysSpoolBuf(PULPSTATE me, PDEVOBJ pdevobj, DWORD dwIndex, IPrintOemDriverPS* pOEMHelp, PDWORD pdwReturn, PSTR pProcedure)
    {
        HRESULT hResult = E_FAIL;
        DWORD   dwLen = 0;
        DWORD   dwSize = 0;

        bool isReplaceOfPScript5DriversPostscript = !me->cIsInjectCommand[dwIndex]; // Prevent PScript5-driver's postscript to be replaced by this plugin
        if (me->_bCancel || me->_bErrorWritingPipe)
        {
            if (!isReplaceOfPScript5DriversPostscript)
            {
                ulplog::LogLineFlush("Print is aborted -> Will not send postscript to inject!");
            }
            // Flags indicate to not inject postscripts any more
            hResult = E_FAIL;
            *pdwReturn = me->_dwWritePipeLastError;
        }
        else
        {
            if (pProcedure == NULL && !isReplaceOfPScript5DriversPostscript)
            {
                // No postscript to inject for this injection point has been defined 
                // -> inject proprietary default DSC comment to mark injection point for ULPSpooler
                pProcedure = MakeLogoPrintPSInjectCommand(me, dwIndex);
            }

            if (NULL != pProcedure && !isReplaceOfPScript5DriversPostscript)
            {
                // A proprietary DSC comment is to be injected -> write it to spool buffer
                dwLen = (DWORD)strnlen(pProcedure, MAX_PADCHARS);
                if (dwLen > 0 && dwLen < MAX_PADCHARS)
                {
                    SetLastError(S_OK);
                    hResult = pOEMHelp->DrvWriteSpoolBuf(pdevobj, pProcedure, dwLen, &dwSize);
                    if (dwLen != dwSize)
                    {
                        ulplog::LogVarUL("Bytes to send", dwLen);
                        ulplog::LogVarUL("Bytes sent", dwSize);
                        ulplog::LogLineFlush("Did not succeed to write all bytes to system-spooler's buffer!");
                    }
                }
                else
                {
                    dwLen = 0;
                    ulplog::LogLineFlush("No bytes to write to system-spooler's buffer!");
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
                    ulplog::LogVarL("LastError from DrvWriteSpoolBuf", *pdwReturn);
                    ulplog::LogVarL("hResult from DrvWriteSpoolBuf", hResult);
                    if (ERROR_SUCCESS == *pdwReturn)
                    {
                        *pdwReturn = ERROR_WRITE_FAULT;
                    }

                    // Make sure we return failure
                    // if the write didn't succeded.
                    if (SUCCEEDED(hResult))
                    {
                        ulplog::LogVarL("hResult from DrvWriteSpoolBuf", hResult);
                        hResult = HRESULT_FROM_WIN32(*pdwReturn);
                    }
                    ulplog::LogVarL("ErrorCode returned", *pdwReturn);
                    ulplog::LogVarL("hResult returned", hResult);
                    ulplog::LogLineFlush("Did not succeed to write system-spooler's buffer!");
                }

                // dwLen should always equal dwSize.
                ASSERT(dwLen == dwSize);
            }
            else
            {
                // No DSC comment is to be injected -> return E_NOTIMPL 
                *pdwReturn = ERROR_NOT_SUPPORTED;
                hResult = E_NOTIMPL;

            }

            if (isReplaceOfPScript5DriversPostscript && NULL != pProcedure)
            {
                ulplog::LogLineFlush("!!! Plugin-postscript has not been written to system-spooler's buffer because PScript5-driver's postscript must not be replaced!");
            }

        }
        return hResult;
    }

    HRESULT ULPCommandInject(PDEVOBJ pdevobj, DWORD dwIndex, PVOID pData, DWORD cbSize,
                             IPrintOemDriverPS* pOEMHelp, PDWORD pdwReturn)
    {
        PSTR    pProcedure = NULL;
        HRESULT hResult = E_FAIL;


        VERBOSE(DLLTEXT("Entering OEMCommand...\r\n"));

        UNREFERENCED_PARAMETER(pData);
        UNREFERENCED_PARAMETER(cbSize);

        int level = -1;

        PULPSTATE me = ((POEMPDEV)pdevobj->pdevOEM)->GetState();

       /* if (!pUlpState->_bAlreadyInitialized) {
            DoInstanceInit(me);
            pUlpState->_bAlreadyInitialized = true;
        }*/

        if (dwIndex > 0 && dwIndex <= MAXCOMMAND && me->cCommandName[dwIndex] != NULL) {
            if (dwIndex == PSINJECT_BEGINPAGESETUP) {
                ulplog::LogLine("");
                ulplog::LogLine("");
            }
            level = LPLOGENTERSECTION(me->cCommandName[dwIndex]);
        }


        switch (dwIndex)
        {
            case PSINJECT_BEGINPAGESETUP:
                ulplog::LogLineFlush("-> increment current page number");
                me->IncCurrentPageNumber();
                VERBOSE(DLLTEXT("OEMCommand PSINJECT_BEGINPAGESETUP\r\n"));
                break;
            case PSINJECT_BEGINSTREAM:
                //cCommandName[dwIndex] = NULL; // Do not write ps-injection-point as DSC-comment
                break;
            case PSINJECT_PSADOBE:
                //cCommandName[dwIndex] = NULL; // Do not write ps-injection-point as DSC-comment
                break;
            case PSINJECT_COMMENTS:
                pProcedure = MakeLogoPrintDSCCommand(me, SETPARAMIDCOMMANDNAME, ""); // PostScript to inject
                me->_bHaveSeenEndComments = true; // Triggers search for SetParamId-command
                ulplog::LogLineFlush("Have seen EndComments -> Trigger search for SetParamId-command when redirecting to LPSpooler");
                if (me->_PADCHARS[0] != '\0')
                {
                    hResult = WritetoSysSpoolBuf(me, pdevobj, dwIndex, pOEMHelp, pdwReturn, pProcedure);
                    pProcedure = NULL;
                    if (SUCCEEDED(hResult))
                    {
                        pProcedure = me->_PADCHARS;
                        ulplog::LogLineFlush("Will add some pad-chars to test overlappping _LOGOPRINT_EOF_DSCCOMMENT!");
                    }
                }
                break;
            case PSINJECT_ENDSTREAM:
                //cCommandName[dwIndex] = NULL; // Do not write ps-injection-point as DSC-comment
                break;
            case PSINJECT_EOF: 
                ulplog::LogLineFlush("-> will stop streaming ...");
                me->_bStreaming = false; // Trigger end of streaming to pipe and logging
                pProcedure = me->_LOGOPRINT_EOF_DSCCOMMENT; // PostScript to inject
                break;
            default:
                VERBOSE(DLLTEXT("PSCommand Default...\r\n"));
                break;
        }

        if (me->_dwPSInjectToFail == dwIndex)
        {
            // Send hResult = E_FAIL for testing 
            hResult = E_FAIL;
            *pdwReturn = me->_dwPSInjectToFailErrorCode;
            ulplog::LogVarL("ErrorCode returned (testing)", *pdwReturn);
            ulplog::LogVarL("hResult returned (testing)", hResult);
            ulplog::LogLine("Will send hResult=E_Fail for testing!");
        }
        else
        {
            hResult = WritetoSysSpoolBuf(me, pdevobj, dwIndex, pOEMHelp, pdwReturn, pProcedure);
        }

        if (level > -1) {
            LPLOGEXITSECTION(level);
        }
        ulplog::LogFlush();

        return hResult;

    }

}