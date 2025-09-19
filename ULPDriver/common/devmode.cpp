//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//  Copyright  1998 - 2003  Microsoft Corporation.  All Rights Reserved.
//
//  FROM:      WDK - OEM Printer Customization Plug-in Samples
//
//  FILE:      Devmode.cpp
//    
//  PURPOSE:   Implementation of Devmode functions shared with OEM UI and OEM rendering modules.
//
//  COMMENT:   No (substantial) modification by UniLogoPrint 2,
//             but removed ui related code (no property pages supported) 
//             and some fields in OEMDEV
//

// This file is wrapped in fdevmode.cpp in other parts of the watermark sample.
// The appropriate precompiled header should be included there.
// #include "precomp.h

#include "debug.h"
#include "devmode.h"

HRESULT hrOEMDevMode(DWORD dwMode, POEMDMPARAM pOemDMParam)
{
    HRESULT hResult     = S_OK;
    POEMDEV pOEMDevIn   = NULL;
    POEMDEV pOEMDevOut  = NULL;

    if (NULL == pOemDMParam)
    {
        ERR(DLLTEXT("DevMode() ERROR_INVALID_PARAMETER 'pOemDMParam'.\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        hResult = E_FAIL;
    }
    else
    {
        // Cast generic (i.e. PVOID) to OEM private devomode pointer type.
        pOEMDevIn = static_cast<POEMDEV>(pOemDMParam->pOEMDMIn);
        pOEMDevOut = static_cast<POEMDEV>(pOemDMParam->pOEMDMOut);

        switch (dwMode)
        {
        case OEMDM_SIZE:
            pOemDMParam->cbBufSize = sizeof(OEMDEV);
            break;

        case OEMDM_DEFAULT:
            pOEMDevOut->dmOEMExtra.dwSize = sizeof(OEMDEV);
            pOEMDevOut->dmOEMExtra.dwSignature = OEM_SIGNATURE;
            pOEMDevOut->dmOEMExtra.dwVersion = OEM_VERSION;
            break;

        case OEMDM_CONVERT:
            ConvertOEMDevmode(pOEMDevIn, pOEMDevOut);
            break;

        case OEMDM_MERGE:
            ConvertOEMDevmode(pOEMDevIn, pOEMDevOut);
            MakeOEMDevmodeValid(pOEMDevOut);
            break;

        default:
            ERR(DLLTEXT("DevMode() ERROR_INVALID_PARAMETER 'dwMode'.\r\n"));
            SetLastError(ERROR_INVALID_PARAMETER);
            hResult = E_FAIL;
            break;
        }
    }



    return hResult;
}


BOOL ConvertOEMDevmode(PCOEMDEV pOEMDevIn, POEMDEV pOEMDevOut)
{
    HRESULT hCopy = S_OK;


    if( (NULL == pOEMDevIn)
        ||
        (NULL == pOEMDevOut)
      )
    {
        ERR(DLLTEXT("ConvertOEMDevmode() invalid parameters.\r\n"));
        return FALSE;
    }

    // Check OEM Signature, if it doesn't match ours,
    // then just assume DMIn is bad and use defaults.
    if(pOEMDevIn->dmOEMExtra.dwSignature == pOEMDevOut->dmOEMExtra.dwSignature)
    {
        VERBOSE(DLLTEXT("Converting private OEM Devmode.\r\n"));

        // Set the devmode defaults so that anything the isn't copied over will
        // be set to the default value.

        // Copy the old structure in to the new using which ever size is the smaller.
        // Devmode maybe from newer Devmode (not likely since there is only one), or
        // Devmode maybe a newer Devmode, in which case it maybe larger,
        // but the first part of the structure should be the same.

        // DESIGN ASSUMPTION: the private DEVMODE structure only gets added to;
        // the fields that are in the DEVMODE never change only new fields get added to the end.

        memcpy(pOEMDevOut, pOEMDevIn, __min(pOEMDevOut->dmOEMExtra.dwSize, pOEMDevIn->dmOEMExtra.dwSize));

        // Re-fill in the size and version fields to indicated 
        // that the DEVMODE is the current private DEVMODE version.
        pOEMDevOut->dmOEMExtra.dwSize       = sizeof(OEMDEV);
        pOEMDevOut->dmOEMExtra.dwVersion    = OEM_VERSION;
    }
    else
    {
        WARNING(DLLTEXT("Unknown DEVMODE signature, pOEMDMIn ignored.\r\n"));

        // Don't know what the input DEVMODE is, so just use defaults.
        pOEMDevOut->dmOEMExtra.dwSize       = sizeof(OEMDEV);
        pOEMDevOut->dmOEMExtra.dwSignature  = OEM_SIGNATURE;
        pOEMDevOut->dmOEMExtra.dwVersion    = OEM_VERSION;
    }

    return SUCCEEDED(hCopy);
}


BOOL MakeOEMDevmodeValid(POEMDEV pOEMDevmode)
{
    if(NULL == pOEMDevmode)
    {
        return FALSE;
    }

    // ASSUMPTION: pOEMDevmode is large enough to contain OEMDEV structure.

    // Make sure that dmOEMExtra indicates the current OEMDEV structure.
    pOEMDevmode->dmOEMExtra.dwSize       = sizeof(OEMDEV);
    pOEMDevmode->dmOEMExtra.dwSignature  = OEM_SIGNATURE;
    pOEMDevmode->dmOEMExtra.dwVersion    = OEM_VERSION;

    //There is nothing to make valid. 

    return TRUE;
}



