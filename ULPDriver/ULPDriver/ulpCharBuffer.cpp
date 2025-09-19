#include "ulpCharBuffer.h"

namespace ulpHelper
{

    void CharBuffer::SetBuffer(size_t textLength)
    {
        m_Size = textLength;
        m_Buffer = new TCHAR[textLength + 1];
        m_bufferSize = (textLength + 1) * sizeof(*m_Buffer);
        ZeroMemory(m_Buffer, m_bufferSize);
        m_bufferAnsi = NULL;
        m_bufferUnicode = NULL;
    }

    TCHAR* CharBuffer::Buffer() { return m_Buffer; }
    size_t CharBuffer::Size() { return m_Size; }    // text-length = character count in Buffer without terminating null char

    CharBuffer::CharBuffer(DWORD textLength)
    {
        SetBuffer(textLength);
    }

    CharBuffer::CharBuffer(const TCHAR* text, DWORD maxsize)
    {
        size_t textLength = _tcsnlen(text, maxsize);
        if (textLength < maxsize)
        {
            SetBuffer(textLength);
            _tcscpy_s(m_Buffer, m_bufferSize, text);
        }
        else
        {
            SetBuffer(0);
        }
        m_bufferAnsi = NULL;
        m_bufferUnicode = NULL;
    }

    CHAR* CharBuffer::GetBufferAnsi()
    {

#ifdef UNICODE
        if (m_bufferAnsi == NULL)
        {
            m_bufferAnsi = new CHAR[m_Size + 1];
            size_t m_bufferAnsiSize = (m_Size + 1) * sizeof(*m_bufferAnsi);
            ZeroMemory(m_bufferAnsi, m_bufferAnsiSize);

            size_t charsConverted;
            wcstombs_s(&charsConverted, m_bufferAnsi, m_bufferAnsiSize, m_Buffer, m_bufferSize);
        }
        return m_bufferAnsi;
#else
        return m_Buffer;

#endif // !UNICODE

    }

    WCHAR* CharBuffer::GetBufferUnicode()
    {

#ifdef UNICODE
        return m_Buffer;
#else
        if (m_bufferUnicode == NULL)
        {
            m_bufferUnicode = new WCHAR[m_Size + 1];
            size_t bufferSizeInBytes = (m_Size + 1) * sizeof(*m_bufferUnicode);
            size_t bufferSizeInWords = bufferSizeInBytes / 2;
            ZeroMemory(m_bufferUnicode, bufferSizeInBytes);

            size_t charsConverted;

            mbstowcs_s(&charsConverted, m_bufferUnicode, bufferSizeInWords, m_Buffer, m_bufferSize);
        }
        return m_bufferUnicode;
#endif // !UNICODE
    }


    CharBuffer::~CharBuffer()
    {
        free();
    }

    void CharBuffer::free()
    {
        if (m_Buffer) 
        {
            delete[] m_Buffer;
            m_Buffer = NULL;
        }
        if (m_bufferAnsi)
        {
            delete[] m_bufferAnsi;
            m_bufferAnsi = NULL;
        }
        if (m_bufferUnicode)
        {
            delete[] m_bufferUnicode;
            m_bufferUnicode = NULL;
        }
    }

}
