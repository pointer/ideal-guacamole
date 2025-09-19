//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//  Copyright  1998 - 2003  Microsoft Corporation.  All Rights Reserved.
//
//  FROM:      WDK - OEM Printer Customization Plug-in Samples
//
//  FILE:      precomp.h
//
//  PURPOSE:   Header files that should be in the precompiled header.
//
//  COMMENT:   No (substantial) modification by UniLogoPrint 2
//

#pragma once

// Necessary for compiling under VC.
#if(!defined(WINVER) || (WINVER < 0x0500))
#error WINVER must be at least 0x0500 
#endif
#if(!defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0500))
#error _WIN32_WINNT must be at least 0x0500 
#endif


// Required header files that shouldn't change often.


#include <STDDEF.H>
#include <STDLIB.H>
#include <OBJBASE.H>
#include <STDARG.H>
#include <STDIO.H>
#include <WINDEF.H>
#include <WINERROR.H>
#include <WINBASE.H>
#include <WINGDI.H>
#include <WINDDI.H>
#include <TCHAR.H>
#include <EXCPT.H>
#include <ASSERT.H>
#include <PRINTOEM.H>
#include <INITGUID.H>
#include <PRCOMOEM.H>

// Safe integer functions to detect integer overflow/underflow conditions:
#include <intsafe.h>

// StrSafe.h needs to be included last
// to disallow unsafe string functions.
#include <STRSAFE.H>

