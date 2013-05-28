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

#ifndef _XENVSS_BYTES_H_
#define _XENVSS_BYTES_H_

#include <string>
using namespace std;

class Bytes
{
public:
    // constructors
    Bytes();
    Bytes(const Bytes& bytes);
    Bytes(size_t capacity);
    Bytes(const unsigned char* bytes, size_t length);
    Bytes(const string& base64);
    Bytes(const char* chars, size_t length, char terminator);

    // destructor
    ~Bytes();

    // operators
    Bytes& operator=(const Bytes& bytes);
    Bytes& operator=(const string& base64);
    const Bytes operator+(const Bytes& bytes) const;
    Bytes& operator+=(unsigned char byte);
    Bytes& operator+=(const Bytes& bytes);
    bool operator==(const Bytes& bytes) const;
    bool operator!=(const Bytes& bytes) const;
    unsigned char& operator[](size_t index);
    unsigned char operator[](size_t index) const;
    operator void*();
    operator const void*() const;
    operator unsigned char*();
    operator const unsigned char*() const;

    // conversions
    Bytes& FromBase64(const string& base64);
    string ToBase64() const;
    string ToString() const;

    // sub-array access
    unsigned char* Ptr(size_t index);
    const unsigned char* Ptr(size_t index) const;

    // settor
    void Set(size_t index, unsigned char val);

    // dimmensions
    void Length(size_t length);
    size_t Length() const;
    size_t Capacity() const;

private:
    void Clear();
    void Resize(size_t new_capacity);
    void Append(const unsigned char* bytes, size_t length);
private:
    unsigned char*  m_bytes;
    size_t          m_capacity;
    size_t          m_length;
};

#endif // _XENVSS_BYTES_H_

