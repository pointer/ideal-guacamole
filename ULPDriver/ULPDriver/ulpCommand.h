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

#include ".\intrface.h"


/////////////////////////////////////////////////////////
//        ProtoTypes
/////////////////////////////////////////////////////////

namespace ulpcommand
{

    void CreateDriverPSDebugFile(PULPSTATE me);

    void InitCommandNames();

    void InitInjectCommand();

    HRESULT ULPCommandInject(PDEVOBJ pdevobj, DWORD dwIndex, PVOID pData, DWORD cbSize, IPrintOemDriverPS* pOEMHelp, PDWORD pdwReturn);

    void WriteDriverDebugFile(PULPSTATE me, const char* cBuffer, DWORD cbBuffer);

    HRESULT WriteToSpoolerPipe(PULPSTATE me, char* cBuffer, DWORD cbBuffer);

    HRESULT ULPWritePrinter(PDEVOBJ, PVOID, DWORD, PDWORD);

}