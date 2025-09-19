#include <Windows.h>
#include <tchar.h>
#include <tchar.h>
#include <string>

#ifndef CHARBUFFER_H
#define CHARBUFFER_H

namespace ulpHelper
{
    class CharBuffer
    {

    private:
        TCHAR* m_Buffer;
        CHAR* m_bufferAnsi;
        WCHAR* m_bufferUnicode;
        rsize_t m_bufferSize;
        size_t  m_Size;

        void SetBuffer(size_t textLength);


    public:

        TCHAR* Buffer();
        size_t Size();    // text-length = character count in Buffer without terminating null char

        CharBuffer(DWORD textLength);
        CharBuffer(const TCHAR* text, DWORD maxsize);

        CHAR* GetBufferAnsi();
        WCHAR* GetBufferUnicode();
        ~CharBuffer();
        void free();

    };

}


#endif