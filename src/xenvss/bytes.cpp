/* Copyright (c) Citrix Systems Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 * 
 * *   Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 * *   Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

#include <windows.h>
#include <stdio.h>
#include "bytes.h"

static __inline char NumToAscii(unsigned char num)
{
    if (num < 26)
        return 'A' + num;
    if (num < 52)
        return 'a' + num - 26;
    if (num < 62)
        return '0' + num - 52;
    if (num == 62)
        return '+';
    if (num == 63)
        return '/';
    return '=';
}
static __inline unsigned char AsciiToBase64(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A';
    if (ch >= 'a' && ch <= 'z')
        return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9')
        return ch - '0' + 52;
    if (ch == '+')
        return 62;
    if (ch == '/')
        return 63;
    return 64;
}

// constructors
Bytes::Bytes() : 
        m_bytes(NULL), m_length(0), m_capacity(0)
{
}
Bytes::Bytes(const Bytes& bytes) : 
        m_bytes(NULL), m_length(bytes.m_length), m_capacity(0)
{
    Resize(bytes.m_capacity);
    memcpy(m_bytes, bytes.m_bytes, m_length);
}
Bytes::Bytes(size_t capacity) : 
        m_bytes(NULL), m_length(0), m_capacity(0)
{
    Resize(capacity);
}
Bytes::Bytes(const unsigned char* bytes, size_t length) : 
        m_bytes(NULL), m_length(length), m_capacity(0)
{
    Resize(length + 1);
    memcpy(m_bytes, bytes, length);
}
Bytes::Bytes(const string& base64) : 
        m_bytes(NULL), m_length(0), m_capacity(0)
{
    FromBase64(base64);
}
Bytes::Bytes(const char* chars, size_t length, char terminator) :
        m_bytes(NULL), m_length(0), m_capacity(0)
{
    Resize(length + 2);
    memcpy(m_bytes, chars, length);
    m_bytes[length] = terminator;
    m_length = length + 1;
}

// destructor
Bytes::~Bytes()
{
    Clear();
}

// operators
Bytes& Bytes::operator=(const Bytes& bytes)
{
    if (this == &bytes)
        return *this;

    Clear();
    Resize(bytes.m_capacity);
    memcpy(m_bytes, bytes.m_bytes, bytes.m_length);
    m_length = bytes.m_length;
    return *this;
}
Bytes& Bytes::operator=(const string& base64)
{
    return FromBase64(base64);
}
const Bytes Bytes::operator+(const Bytes& bytes) const 
{
    return Bytes(*this) += bytes;
}
Bytes& Bytes::operator+=(unsigned char byte)
{
    Append(&byte, 1);
    return *this;
}
Bytes& Bytes::operator+=(const Bytes& bytes)
{
    Append(bytes.m_bytes, bytes.m_length);
    return *this;
}
bool Bytes::operator==(const Bytes& bytes) const
{
    if (this == &bytes)
        return true;
    if (m_length != bytes.m_length)
        return false;
    return memcmp(m_bytes, bytes.m_bytes, m_length) == 0;
}
bool Bytes::operator!=(const Bytes& bytes) const
{
    return !(*this == bytes);
}
unsigned char& Bytes::operator[](size_t index)
{
    if (index >= m_length)
        throw "IndexOutOfBounds";
    return m_bytes[index];
}
unsigned char Bytes::operator[](size_t index) const
{
    if (index >= m_length)
        throw "IndexOutOfBounds";
    return m_bytes[index];
}
Bytes::operator void*()
{
    return (void*)m_bytes;
}
Bytes::operator const void*() const
{
    return (const void*)m_bytes;
}
Bytes::operator unsigned char*()
{
    return m_bytes;
}
Bytes::operator const unsigned char*() const
{
    return (const unsigned char*)m_bytes;
}

// conversions
Bytes& Bytes::FromBase64(const string& base64)
{
    Clear();
    if (base64.length() % 4)
        throw "InvalidInputLength";
    Resize( (base64.length() / 4) * 3 );
    for (size_t i = 0; i < base64.length(); i += 4) {
        unsigned char val[4] =
        {
            AsciiToBase64(base64[i]),
            AsciiToBase64(base64[i + 1]),
            AsciiToBase64(base64[i + 2]),
            AsciiToBase64(base64[i + 3])
        };
        m_bytes[m_length++] = (val[0] << 2) | (val[1] >> 4);
        if (val[2] == 64) break;
        m_bytes[m_length++] = (val[1] << 4) | (val[2] >> 2);
        if (val[3] == 64) break;
        m_bytes[m_length++] = (val[2] << 6) | val[3];
    }
    return *this;
}
string Bytes::ToBase64() const
{
    string retval;
    for (size_t i = 0; i < m_length; i+= 3) {
        unsigned char in[3] = { 0, 0, 0 };
        int len = 0;
        for (size_t j = 0; j < 3; ++j) {
            if (i + j < m_length) {
                in[j] = m_bytes[i + j];
                ++len;
            }
        }
        char out[5] = { '=', '=', '=', '=', 0 };
        out[0] = NumToAscii( in[0] >> 2 );
        out[1] = NumToAscii( ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) );
        out[2] = len > 1 ? NumToAscii( ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ) : '=';
        out[3] = len > 2 ? NumToAscii( (in[2] & 0x3f) ) : '=';
        retval += out;
    }
    return retval;
}
string Bytes::ToString() const
{
    string retval;
    for (size_t i = 0; i < m_length; ++i) {
        char tmp[3] = { 0, 0, 0 };
        _snprintf_s(tmp, sizeof(tmp), 2, "%02x", m_bytes[i]);
        retval += tmp;
    }
    return retval;
}

// sub-array access
unsigned char* Bytes::Ptr(size_t index)
{
    if (index >= m_capacity)
        throw "IndexOutOfBounds";
    return &m_bytes[index];
}
const unsigned char* Bytes::Ptr(size_t index) const
{
    if (index >= m_capacity)
        throw "IndexOutOfBounds";
    return (const unsigned char*)&m_bytes[index];
}

// settor
void Bytes::Set(size_t index, unsigned char val)
{
    if (index >= m_capacity)
        throw "IndexOutOfBounds";
    m_bytes[index] = val;
}

// dimmensions
void Bytes::Length(size_t length)
{
    if (length >= m_capacity)
        throw "InvalidLength";
    m_length = length;
}
size_t Bytes::Length() const
{
    return m_length;
}
size_t Bytes::Capacity() const
{
    return m_capacity;
}

void Bytes::Clear()
{
    if (m_bytes)
        delete [] m_bytes;
    m_bytes = NULL;
    m_length = 0;
    m_capacity = 0;
}
void Bytes::Resize(size_t new_capacity)
{
    unsigned char* new_bytes = new unsigned char[new_capacity];
    if (new_bytes == NULL)
        throw "OutOfMemory";
    memset(new_bytes, 0, new_capacity);
    size_t new_length = 0;
    if (m_bytes) {
        new_length = m_length;
        if (new_length > new_capacity)
            new_length = new_capacity;
        memcpy(new_bytes, m_bytes, new_length);
        delete [] m_bytes;
    }
    m_bytes = new_bytes;
    m_length = new_length;
    m_capacity = new_capacity;
}
void Bytes::Append(const unsigned char* bytes, size_t length)
{
    if (m_length + length >= m_capacity)
        Resize(m_capacity + length + 1);
    for (size_t i = 0; i < length; ++i)
        m_bytes[m_length + i] = bytes[i];
    m_length += length;
}
