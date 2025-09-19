//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//  Copyright  1998 - 2003  Microsoft Corporation.  All Rights Reserved.
//
//  FROM:      WDK - OEM Printer Customization Plug-in Samples
//
//  FILE:       Devmode.h
//    
//  PURPOSE:   Define common data types, and external function prototypes
//               for devmode functions.
//
//  COMMENT:   No (substantial) modification by UniLogoPrint 2,
//             but removed ui related code (no property pages supported) 
//             and some fields in OEMDEV
//
#pragma once 

////////////////////////////////////////////////////////
//      OEM Devmode Defines
////////////////////////////////////////////////////////



////////////////////////////////////////////////////////
//      OEM Devmode Type Definitions
////////////////////////////////////////////////////////

typedef struct tagOEMDEV
{
    OEM_DMEXTRAHEADER   dmOEMExtra;
} OEMDEV;

typedef OEMDEV *POEMDEV;

typedef const OEMDEV *PCOEMDEV;



/////////////////////////////////////////////////////////
//        ProtoTypes
/////////////////////////////////////////////////////////

HRESULT hrOEMDevMode(DWORD dwMode, POEMDMPARAM pOemDMParam);
BOOL ConvertOEMDevmode(PCOEMDEV pOEMDevIn, POEMDEV pOEMDevOut);
BOOL MakeOEMDevmodeValid(POEMDEV pOEMDevmode);

