//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//  Copyright  1997 - 2003  Microsoft Corporation.  All Rights Reserved.
//
//  FILE:   ulpOemPDEV.H
//    
//
//  PURPOSE:    Define the COemPDEV class which stores the private
//              PDEV for the driver.
//

#pragma once 

#include "CUlpCommandHandler.h"


class COemPDEV
{
public:

    __stdcall COemPDEV(PWSTR pPrinterName)
    {

        // Note that since our parent has AddRef'd the UNIDRV interface,
        // we don't do so here since our scope is identical to our
        // parent.
        //

        VERBOSE("In COemPDEV constructor...");
        _commandHandler = new CUlpCommandHandler(pPrinterName);
    }

    __stdcall ~COemPDEV(void)
    {
        VERBOSE("In COemPDEV destructor...");
        if (_commandHandler != NULL) {
            delete _commandHandler;
        }
        _commandHandler = NULL;
    }



    void __stdcall TransferCommandHandlerTo(COemPDEV* pOemPDEV)
    {
        VERBOSE("COemPDEV::TransferCommandHandler");

        // Transfer the commandHandler from this instance to pOemPDEV
        pOemPDEV->_commandHandler = _commandHandler;
        _commandHandler = NULL;
        // commandHandler is removed from this instance and nulled(!) 
        // -> when destructor of this instance runs the commandHandler-instance will not be deleted!
    }

    PULPCOMMANDHANDLER __stdcall GetCommandHandler() {
        return _commandHandler;
    }

private:
    PULPCOMMANDHANDLER _commandHandler;

};
typedef COemPDEV* POEMPDEV;
