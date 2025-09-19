#pragma once

#include "ulpLog.h"
#include "ulpHelper.h"
#include "ulpSpoolerPipe.h"

const DWORD MAXSIZEDRIVERJOBID = 20;   //buffer size large enough for driver's jobId
const DWORD MAXSIZEPAGENUMBER = 10;    //buffer size large enough for pagenumber
const DWORD PLACEHOLDERMAXSIZE = 255;  //buffer size large enough to hold any used placeholder
const DWORD MAX_PADCHARS = 8192;        //maximum number of padding chars to test overlapping _LOGOPRINT_EOF_DSCCOMMENT
const int MAXCOMMAND = 201;
const DWORD DSCCOMMANDMAXSIZE = 150;   //buffer size large enough to hold command-pattern
const DWORD SETPARAMIDCOMMANDNAMESIZE = 30;   //buffer size large enough to SetParamId-command

class CUlpCommand
{




public:


    // Flag indicating wether static fields are already initialized
    static bool bStaticFieldsInitialized = false;

    // Array of propietary DSC comments used to mark injection points for evaluation by ULPSpooler
    const char* cCommandName[MAXCOMMAND + 1];
    bool cIsInjectCommand[MAXCOMMAND + 1];

    // Propietary DSC pattern used for poastscript injections (see HKLM-LogoPrint2-Key LogoPrintDSCCommandCStylePattern)
    CHAR LOGOPRINT_DSCCOMMAND[DSCCOMMANDMAXSIZE];

    // How the propietary DSC comment for SetParamId-command starts (is used to identify a SetParamId-command)
    static CHAR SETPARAMIDCOMMANDPREFIX[PLACEHOLDERMAXSIZE];

    // Propietary DSC comment for SetParamId-command 
    static CHAR SETPARAMIDCOMMANDNAME[SETPARAMIDCOMMANDNAMESIZE];

    // Flag indicating if driver currently streams postscript to ULPSpooler.exe
    // Is set to true when InitCommand() is called.
    // Will be set to false when LOGOPRINT_EOFSTREAM_MARK is injected postscript-stream
    // and found by ULPWritePrinter().
    bool  _bStreaming;

    // Flag indicating if ULPSpooler.exe closed pipe to abort spooling
    bool  _bCancel;

    // Flag indicating if there has been an error writeing to the pipe 
    bool  _bErrorWritingPipe;

    // LastError when writing to pipe 
    DWORD _dwWritePipeLastError;

    // Flag indicating if postscript injection point PSINJECT_COMMENTS has been reached
    // Triggers search for SetParamId-command with format "%UCSLogoPrint: SetParameterId(nnn)" (see also SETPARAMIDCOMMANDNAME)
    bool  _bHaveSeenEndComments;

    // Flag indicating if already has been checked wether SetParamId-command is contained in postscript
    // Used to check only once
    bool  _bCheckWriteMapIdFile;

    // Received postscript sent by system-spooler will be written/logged to this ofstream
    std::ofstream _streamPSDebugFile;

    // Flag indicating if postscript sent by system-spooler has to be logged to _streamPSDebugFile
    bool _bWriteToPSDebugFile;

    // Flag indicating whether this instance is initialized
    bool _bAlreadyInitialized;


    // DriverJob-Id used:
    // + in injected postscript as marker
    // + will be written to output (MapId-file), when printing application does not send a SetParamId-command
    //   and allows to identify the parameter-file to be used by ULPSpooler.exe
    // + will be passed to ULPSpooler.exe to identify the parameter-file when SetParamId-command is not sent 
    //   by printign application
    long _lDriverJobId;
    CHAR _cbDriverJobId[MAXSIZEDRIVERJOBID];

    // Current page number (used as parameter in injected postscript)
    int  _iCurrentPageNumber;
    CHAR _cbCurrentPageNumber[MAXSIZEPAGENUMBER];

    // Buffer for Propietary DSC postscript to inject
    CHAR _bufferPSToInject[PLACEHOLDERMAXSIZE];

    // Propietary DSC comment used to signal EOF to ULPWritePrinter()
    CHAR _LOGOPRINT_EOF_DSCCOMMENT[PLACEHOLDERMAXSIZE];

    // Variables for Knuth-Morris-Pratt algorithmen used to find _LOGOPRINT_EOF_DSCCOMMENT in stream
    DWORD _dwCharPosComment;
    int _kmpNext[PLACEHOLDERMAXSIZE];
    DWORD _dwCommentLength;

    //Chars padded to postcript to let _LOGOPRINT_EOF_DSCCOMMENT overlap spooler-buffer (for testing)
    CHAR _PADCHARS[MAX_PADCHARS];

    //PSInjectCommand that has to fail and send hResult=E_FAIL (for testing)
    DWORD _dwPSInjectToFail;

    //Error number returned to system when PSInjectCommand fails (for testing)
    DWORD _dwPSInjectToFailErrorCode;


    // Increments and sets current page number
    void IncCurrentPageNumber()
    {
        SetCurrentPageNumber(_iCurrentPageNumber + 1);
    };

    // Fills char-buffer _cbCurrentPageNumber with current page number
    void SetCurrentPageNumber(int n)
    {
        _iCurrentPageNumber = n;
        ZeroMemory(_cbCurrentPageNumber, sizeof(_cbCurrentPageNumber));
        StringCbPrintfA(_cbCurrentPageNumber, sizeof(_cbCurrentPageNumber), "%d", n);
        ulplog::LogVarUL("Current Page Number", n);
        ulplog::LogFlush();
    }

    // Creates driver-job-id _lDriverJobId and fills char-buffer _cbDriverJobId 
    void CreateDriverJobId()
    {
        ZeroMemory(_cbDriverJobId, sizeof(_cbDriverJobId));
        long lowerBoundDriverJobId = 10000000;
        long upperBoundDriverJobId = 99999999;
        srand((time(NULL) - 1538123990) & 0x8FFFFFFF);
        _lDriverJobId = ((rand() * RAND_MAX + rand()) % (upperBoundDriverJobId - lowerBoundDriverJobId)) + lowerBoundDriverJobId;
        StringCbPrintfA(_cbDriverJobId, sizeof(_cbDriverJobId), "%d", _lDriverJobId);
    }

    // Resets this module before start of print-job
    void InitCommand()
    {
        // Initialize page number
        ulplog::LogLineFlush("Setting current page number to 0.");
        SetCurrentPageNumber(0);

        _bStreaming = true;

        // Create driver-job-id _lDriverJobId and fill char-buffer _cbDriverJobId 
        ulplog::LogLineFlush("Creating DriverJobId ...");
        CreateDriverJobId();
        ulplog::LogVar("_cbDriverJobId", _cbDriverJobId);

        ulplog::LogLineFlush("Initializing static fields ...");
        InitStaticFields();

        //Create current LOGOPRINT_EOFSTREAM_MARK
        ulplog::LogLineFlush("Setting _LOGOPRINT_EOF_DSCCOMMENT ...");
        ZeroMemory(_LOGOPRINT_EOF_DSCCOMMENT, sizeof(_LOGOPRINT_EOF_DSCCOMMENT));
        StringCbPrintfA(_LOGOPRINT_EOF_DSCCOMMENT, sizeof(_LOGOPRINT_EOF_DSCCOMMENT), LOGOPRINT_DSCCOMMAND, cCommandName[PSINJECT_ENDSTREAM], "", _cbDriverJobId);
        ulplog::LogVar("_LOGOPRINT_EOF_DSCCOMMENT", _LOGOPRINT_EOF_DSCCOMMENT);


        _dwCommentLength = (DWORD)strnlen_s(_LOGOPRINT_EOF_DSCCOMMENT, sizeof(_LOGOPRINT_EOF_DSCCOMMENT));
        ulplog::LogLineFlush("Preprocessing KnuthMorrisPratt ...");
        PreprocessKnuthMorrisPratt(_LOGOPRINT_EOF_DSCCOMMENT, _dwCommentLength, _kmpNext);

        int iPadCommentCharCount = ulphelper::ReadLogoPrintRegInt(HKEY_CURRENT_USER, _T("PadCommentCharCount"), 0);
        if (iPadCommentCharCount > 0 && iPadCommentCharCount < MAX_PADCHARS)
        {
            ulplog::LogLineFlush("Preparing pad comment for testing buffer overlap ...");
            ZeroMemory(_PADCHARS, sizeof(_PADCHARS));
            const char* PADCHARPattern = "%%UCSLogoPrint PAD\r\n";
            int patternLength = (int)strnlen_s(PADCHARPattern, 200);
            int numPatterns = iPadCommentCharCount / (int)patternLength;

            char* nextPosToCopyTo = _PADCHARS;
            int remainingBufferSize = sizeof(_PADCHARS);

            while (numPatterns > 0)
            {
                strcpy_s(nextPosToCopyTo, remainingBufferSize, PADCHARPattern);
                nextPosToCopyTo += patternLength;
                remainingBufferSize -= patternLength;
                numPatterns--;
            }
        }

        DWORD _dwPSInjectToFail = ulphelper::ReadLogoPrintRegInt(HKEY_CURRENT_USER, _T("PSInjectToFail"), 0);
        if (_dwPSInjectToFail > 0)
        {
            ulplog::LogLineFlush("Preparing PSIbnject to fail for test purposes ...");
            _dwPSInjectToFail = _dwPSInjectToFail;
            _dwPSInjectToFailErrorCode = ulphelper::ReadLogoPrintRegInt(HKEY_CURRENT_USER, _T("PSInjectToFailErrorCode"), ERROR_BAD_PIPE); // default ERROR_BAD_PIPE
        }
        ulplog::LogLine("End of InitCommand()");
        ulplog::LogLine("");
        ulplog::LogLineFlush("");
    }

    // Opens a debug-file to log to, provided that a filename is specified in HKCU-LogoPrint2-Key DriverPSDebugFile
    void CreateDriverPSDebugFile()
    {
        ulphelper::CharBuffer* driverPSFileName = ulphelper::ReadLogoPrintRegStr(HKEY_CURRENT_USER, _T("LPDriverPSDebugFile"));
        if (driverPSFileName != NULL && driverPSFileName->Size() > 0)
        {
            char filename[MAX_PATH + 10];
            ZeroMemory(filename, sizeof(filename));

            sprintf_s(filename, sizeof(filename), "%s_%ld.txt", driverPSFileName->GetBufferAnsi(), _lDriverJobId);

            try
            {
                _streamPSDebugFile.open(filename, std::ios::binary);
                _bWriteToPSDebugFile = _streamPSDebugFile.is_open();
            }
            catch (const std::exception& e)
            {
                LPLOGERROR("Error opening ps debug file", e);
                _bWriteToPSDebugFile = false;
            }
            ulplog::LogLineParts("Will write ps output to '", filename, "' for debugging", NULL);
        }
        if (driverPSFileName != NULL)
        {
            driverPSFileName->free();
        }
    }


    __stdcall CUlpState(void)
    {

        // Note that since our parent has AddRef'd the UNIDRV interface,
        // we don't do so here since our scope is identical to our
        // parent.
        //

        VERBOSE("In CUlpState constructor...");


        _bStreaming = false;
        _bCancel = false;
        _bErrorWritingPipe = false;
        _dwWritePipeLastError = 0;
        _bWriteToPSDebugFile = false;
        _bHaveSeenEndComments = false;
        _bCheckWriteMapIdFile = true;
        //_streamPSDebugFile;
        _bWriteToPSDebugFile = false;
        _lDriverJobId = 0;
        _iCurrentPageNumber = 0;
        _dwCharPosComment = 0;
        _dwPSInjectToFail = 0;
        _dwPSInjectToFailErrorCode = 0;
        _bAlreadyInitialized = false;

        ZeroMemory(_cbDriverJobId, sizeof(_cbDriverJobId));
        ZeroMemory(_cbCurrentPageNumber, sizeof(_cbCurrentPageNumber));
        ZeroMemory(_bufferPSToInject, sizeof(_bufferPSToInject));
        ZeroMemory(_LOGOPRINT_EOF_DSCCOMMENT, sizeof(_LOGOPRINT_EOF_DSCCOMMENT));
        ZeroMemory(_PADCHARS, sizeof(_PADCHARS));

        ulplog::InitLog(_T("LogoPrint_LPDriver"));

        LPLOGENTERSECTION("DoInstanceInit is called (V1.20)");

        InitCommand();

        ulppipe2spooler::InitAndStartSpooler(_lDriverJobId);

        CreateDriverPSDebugFile();

        ulplog::LogVar("SETPARAMIDCOMMANDPREFIX", SETPARAMIDCOMMANDPREFIX);
        ulplog::LogVar("SETPARAMIDCOMMANDNAME", SETPARAMIDCOMMANDNAME);

        ulplog::LogLineFlush("End of DoInstanceInit");

    }

    __stdcall ~CUlpState(void)
    {
        VERBOSE("In CUlpState destructor...");

    }


private:

    void InitCommandNames()
    {
        // Init command names
        ZeroMemory(cCommandName, sizeof(cCommandName));
        cCommandName[PSINJECT_BEGINSTREAM] = "PSINJECT_BEGINSTREAM";
        cCommandName[PSINJECT_PSADOBE] = "PSINJECT_PSADOBE";
        cCommandName[PSINJECT_PAGESATEND] = "PSINJECT_PAGESATEND";
        cCommandName[PSINJECT_PAGES] = "PSINJECT_PAGES";
        cCommandName[PSINJECT_DOCNEEDEDRES] = "PSINJECT_DOCNEEDEDRES";
        cCommandName[PSINJECT_DOCSUPPLIEDRES] = "PSINJECT_DOCSUPPLIEDRES";
        cCommandName[PSINJECT_PAGEORDER] = "PSINJECT_PAGEORDER";
        cCommandName[PSINJECT_ORIENTATION] = "PSINJECT_ORIENTATION";
        cCommandName[PSINJECT_BOUNDINGBOX] = "PSINJECT_BOUNDINGBOX";
        cCommandName[PSINJECT_DOCUMENTPROCESSCOLORS] = "PSINJECT_DOCUMENTPROCESSCOLORS";
        cCommandName[PSINJECT_COMMENTS] = "PSINJECT_COMMENTS";
        cCommandName[PSINJECT_BEGINDEFAULTS] = "PSINJECT_BEGINDEFAULTS";
        cCommandName[PSINJECT_ENDDEFAULTS] = "PSINJECT_ENDDEFAULTS";
        cCommandName[PSINJECT_BEGINPROLOG] = "PSINJECT_BEGINPROLOG";
        cCommandName[PSINJECT_ENDPROLOG] = "PSINJECT_ENDPROLOG";
        cCommandName[PSINJECT_BEGINSETUP] = "PSINJECT_BEGINSETUP";
        cCommandName[PSINJECT_ENDSETUP] = "PSINJECT_ENDSETUP";
        cCommandName[PSINJECT_TRAILER] = "PSINJECT_TRAILER";
        cCommandName[PSINJECT_EOF] = "PSINJECT_EOF";
        cCommandName[PSINJECT_ENDSTREAM] = "PSINJECT_ENDSTREAM";
        cCommandName[PSINJECT_DOCUMENTPROCESSCOLORSATEND] = "PSINJECT_DOCUMENTPROCESSCOLORSATEND";
        cCommandName[PSINJECT_PAGENUMBER] = "PSINJECT_PAGENUMBER";
        cCommandName[PSINJECT_BEGINPAGESETUP] = "PSINJECT_BEGINPAGESETUP";
        cCommandName[PSINJECT_ENDPAGESETUP] = "PSINJECT_ENDPAGESETUP";
        cCommandName[PSINJECT_PAGETRAILER] = "PSINJECT_PAGETRAILER";
        cCommandName[PSINJECT_PLATECOLOR] = "PSINJECT_PLATECOLOR";
        cCommandName[PSINJECT_SHOWPAGE] = "PSINJECT_SHOWPAGE";
        cCommandName[PSINJECT_PAGEBBOX] = "PSINJECT_PAGEBBOX";
        cCommandName[PSINJECT_ENDPAGECOMMENTS] = "PSINJECT_ENDPAGECOMMENTS";
        cCommandName[PSINJECT_VMSAVE] = "PSINJECT_VMSAVE";
        cCommandName[PSINJECT_VMRESTORE] = "PSINJECT_VMRESTORE";
    }

    void InitInjectCommand()
    {
        cIsInjectCommand[PSINJECT_BEGINSTREAM] = true;
        cIsInjectCommand[PSINJECT_PSADOBE] = true;
        cIsInjectCommand[PSINJECT_PAGESATEND] = false; // Will not inject but replace!
        cIsInjectCommand[PSINJECT_PAGES] = false; // Will not inject but replace!
        cIsInjectCommand[PSINJECT_DOCNEEDEDRES] = true;
        cIsInjectCommand[PSINJECT_DOCSUPPLIEDRES] = true;
        cIsInjectCommand[PSINJECT_PAGEORDER] = false; // Will not inject but replace!
        cIsInjectCommand[PSINJECT_ORIENTATION] = false; // Will not inject but replace!
        cIsInjectCommand[PSINJECT_BOUNDINGBOX] = false; // Will not inject but replace!
        cIsInjectCommand[PSINJECT_DOCUMENTPROCESSCOLORS] = false; // Will not inject but replace!
        cIsInjectCommand[PSINJECT_COMMENTS] = true;
        cIsInjectCommand[PSINJECT_BEGINDEFAULTS] = true;
        cIsInjectCommand[PSINJECT_ENDDEFAULTS] = true;
        cIsInjectCommand[PSINJECT_BEGINPROLOG] = true;
        cIsInjectCommand[PSINJECT_ENDPROLOG] = true;
        cIsInjectCommand[PSINJECT_BEGINSETUP] = true;
        cIsInjectCommand[PSINJECT_ENDSETUP] = true;
        cIsInjectCommand[PSINJECT_TRAILER] = true;
        cIsInjectCommand[PSINJECT_EOF] = true;
        cIsInjectCommand[PSINJECT_ENDSTREAM] = true;
        cIsInjectCommand[PSINJECT_DOCUMENTPROCESSCOLORSATEND] = false; // Will not inject but replace!
        cIsInjectCommand[PSINJECT_PAGENUMBER] = false; // Will not inject but replace! 
        cIsInjectCommand[PSINJECT_BEGINPAGESETUP] = true;
        cIsInjectCommand[PSINJECT_ENDPAGESETUP] = true;
        cIsInjectCommand[PSINJECT_PAGETRAILER] = true;
        cIsInjectCommand[PSINJECT_PLATECOLOR] = false; // Will not inject but replace!
        cIsInjectCommand[PSINJECT_SHOWPAGE] = false; // Will not inject but replace!
        cIsInjectCommand[PSINJECT_PAGEBBOX] = false; // Will not inject but replace!
        cIsInjectCommand[PSINJECT_ENDPAGECOMMENTS] = true;
        cIsInjectCommand[PSINJECT_VMSAVE] = true;
        cIsInjectCommand[PSINJECT_VMRESTORE] = true;
    }

    void InitStaticFields()
    {
        //Read reag-values only once
        ZeroMemory(LOGOPRINT_DSCCOMMAND, sizeof(LOGOPRINT_DSCCOMMAND));
        ulphelper::CharBuffer* pattern = ulphelper::ReadLogoPrintRegStr(HKEY_LOCAL_MACHINE, _T("DSCCommandCStylePattern"));    // something like  %%UCSLogoPrint %s(%s) [%d]
        if (pattern != NULL) {
            strcpy_s(LOGOPRINT_DSCCOMMAND, sizeof(LOGOPRINT_DSCCOMMAND), pattern->GetBufferAnsi());
        }
        else {
            strcpy_s(LOGOPRINT_DSCCOMMAND, sizeof(LOGOPRINT_DSCCOMMAND), "%%UCSLogoPrint %s(%s) [%s]");
        }

        size_t textLength = strnlen_s(LOGOPRINT_DSCCOMMAND, sizeof(LOGOPRINT_DSCCOMMAND));
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

        ZeroMemory(SETPARAMIDCOMMANDNAME, sizeof(SETPARAMIDCOMMANDNAME));
        ulphelper::CharBuffer* setParamIdDSCCommandName = ulphelper::ReadLogoPrintRegStr(HKEY_LOCAL_MACHINE, _T("DSCCommandSetParamId"));
        strcpy_s(SETPARAMIDCOMMANDNAME, sizeof(SETPARAMIDCOMMANDNAME), setParamIdDSCCommandName->GetBufferAnsi());

        ZeroMemory(SETPARAMIDCOMMANDPREFIX, sizeof(SETPARAMIDCOMMANDPREFIX));
        ulphelper::CharBuffer* DSC_Prefix = ulphelper::ReadLogoPrintRegStr(HKEY_LOCAL_MACHINE, _T("DSCPrefix"));
        sprintf_s(SETPARAMIDCOMMANDPREFIX, sizeof(SETPARAMIDCOMMANDPREFIX), "\r\n%s%s", DSC_Prefix->GetBufferAnsi(), setParamIdDSCCommandName->GetBufferAnsi());

        if (DSC_Prefix)
        {
            DSC_Prefix->free();
        }
        if (setParamIdDSCCommandName)
        {
            setParamIdDSCCommandName->free();
        }

        InitCommandNames();
        InitInjectCommand();
    }


public:

};
//typedef CUlpCommand* PULPCOMMAND;

