//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//  Copyright  1997 - 2003  Microsoft Corporation.  All Rights Reserved.
//
//  FROM:      WDK - OEM Printer Customization Plug-in Samples
//
//  FILE:      Intrface.H
//
//  PURPOSE:   Define COM interface for User Mode Printer Customization DLL.
//
//  COMMENT:   No (substantial) modification by UniLogoPrint 2, but
//             + implmenting IPrintOemPS2 (instead of IPrintOemPS)
// 
//

#pragma once

////////////////////////////////////////////////////////////////////////////////
//
//  IULPDriverPS
//
//  Interface for PostScript OEM sample rendering module
//
class IULPDriverPS : public IPrintOemPS2
{
public:

    // *** IUnknown methods ***

    STDMETHOD(QueryInterface) (THIS_ REFIID riid, LPVOID FAR* ppvObj);

    STDMETHOD_(ULONG,AddRef)  (THIS);

    // the _At_ tag here tells prefast that once release 
    // is called, the memory should not be considered leaked
    _At_(this, __drv_freesMem(object)) 
    STDMETHOD_(ULONG,Release) (THIS);

    //
    // Method for publishing Driver interface.
    //
    STDMETHOD(PublishDriverInterface)(THIS_ IUnknown *pIUnknown);

    //
    // Method for OEM to specify DDI hook out
    //

    STDMETHOD(EnableDriver)  (THIS_ DWORD           DriverVersion,
                                    DWORD           cbSize,
                                    PDRVENABLEDATA  pded);

    //
    // Method to notify OEM plugin that it is no longer required
    //

    STDMETHOD(DisableDriver) (THIS);

    //
    // Method for OEM to contruct its own PDEV
    //

    STDMETHOD(EnablePDEV)    (THIS_ PDEVOBJ         pdevobj,
                                    _In_ PWSTR      pPrinterName,
                                    ULONG           cPatterns,
                                    HSURF          *phsurfPatterns,
                                    ULONG           cjGdiInfo,
                                    GDIINFO        *pGdiInfo,
                                    ULONG           cjDevInfo,
                                    DEVINFO        *pDevInfo,
                                    DRVENABLEDATA  *pded,
                                    OUT PDEVOEM    *pDevOem);

    //
    // Method for OEM to free any resource associated with its PDEV
    //

    STDMETHOD(DisablePDEV)   (THIS_ PDEVOBJ         pdevobj);

    //
    // Method for OEM to transfer from old PDEV to new PDEV
    //

    STDMETHOD(ResetPDEV)     (THIS_ PDEVOBJ         pdevobjOld,
                                    PDEVOBJ        pdevobjNew);


    //
    // Get OEM dll related information
    //

    STDMETHOD(GetInfo) (THIS_ DWORD   dwMode,
                              PVOID   pBuffer,
                              DWORD   cbSize,
                              PDWORD  pcbNeeded);
    //
    // OEMDevMode
    //

    STDMETHOD(DevMode) (THIS_ DWORD       dwMode,
                              POEMDMPARAM pOemDMParam);

    //
    // OEMCommand - PSCRIPT only, return E_NOTIMPL on Unidrv
    //

    STDMETHOD(Command) (THIS_ PDEVOBJ     pdevobj,
                              DWORD       dwIndex,
                              PVOID       pData,
                              DWORD       cbSize,
                              OUT DWORD   *pdwResult);

    //

    //
    // Additional IPrintOemPS2 methods implemeted by UniLogoPrint 2
    //

    //
    // Method for plugin to hook out spooler's WritePrinter API so it
    // can get access to output data PostScript driver is generating
    //
    // At DrvEnablePDEV time, PostScript driver will call this function with
    // pdevobj = NULL, pBuf = NULL, cbBuffer = 0 to detect if the plugin
    // implements this function. Plugin should return S_OK to indicate it is
    // implementing this function, or return E_NOTIMPL otherwise.
    //
    // In pcbWritten, plugins should return the number of bytes written to the
    // spooler's WritePrinter function. Zero doesn't carry a special meaning,
    // errors must be reported through the returned HRESULT.
    //

    STDMETHOD(WritePrinter) (THIS_   PDEVOBJ    pdevobj,
        PVOID      pBuf,
        DWORD      cbBuffer,
        PDWORD     pcbWritten);

    //
    // Method for plugin to implement if it wants to be called to get the chance
    // to override some PDEV settings such as paper margins.
    // Plugins that recognize the adjustment type should return S_OK.
    // If the adjustment type is unrecognized, they should return S_FALSE
    // and not E_NOTIMPL, this code should be reserved for the COM meaning.
    // If the plugin fails the call, it should return E_FAIL.
    // The chain of plugins will be called until a plugin returns S_OK or
    // any failure code other than E_NOTIMPL, in other words, until the first
    // plugin that is designed to handle the adjustment is found.
    //

    STDMETHOD(GetPDEVAdjustment) (THIS_ PDEVOBJ    pdevobj,
        DWORD      dwAdjustType,
        PVOID      pBuf,
        DWORD      cbBuffer,
        OUT BOOL  *pbAdjustmentDone);


    IULPDriverPS() 
    { 
        m_cRef = 1; m_pOEMHelp = NULL; 
    };

    ~IULPDriverPS();

protected:
    LONG                m_cRef;
    IPrintOemDriverPS*  m_pOEMHelp;



};
