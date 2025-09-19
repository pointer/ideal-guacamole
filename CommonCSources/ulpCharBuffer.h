#include <Windows.h>
#include <winnt.h>
#include <tchar.h>
#include <winBase.h>
#include <tchar.h>
#include <string>

#ifndef CHARBUFFER_H
#define CHARBUFFER_H

namespace ulphelper
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

        CHAR* CharBuffer::GetBufferAnsi();
        WCHAR* CharBuffer::GetBufferUnicode();
        ~CharBuffer();
        void free();

    };

}


#endif