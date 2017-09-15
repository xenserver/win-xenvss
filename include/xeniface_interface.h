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

#ifndef _XENSTORE_INTERFACE_H_
#define _XENSTORE_INTERFACE_H_

// #define INITGUID in 1 source file before including this file

#include "xeniface_ioctls.h"

#include <string>
#include <vector>
typedef std::string                 String;
typedef std::vector< std::string >  StringVct;

#include <setupapi.h>
#pragma comment (lib , "setupapi.lib" )

static void ____DebugPrint(const char* fmt, ...)
{
    char    buffer[1024];
    va_list args;

    va_start(args, fmt);
    vsprintf_s(buffer, sizeof(buffer)/sizeof(buffer[0]), fmt, args);
    va_end(args);

    OutputDebugString(buffer);
}

#ifdef DBG
#define DebugPrint(x) ____DebugPrint x
#else
#define DebugPrint(x) (VOID)(x)
#endif

class XenIfaceItf
{
public:
    XenIfaceItf() : m_handle(INVALID_HANDLE_VALUE)
    {
        HDEVINFO                            Info;
        SP_DEVICE_INTERFACE_DATA            ItfData;
        PSP_DEVICE_INTERFACE_DETAIL_DATA    ItfDetailData;
        ULONG                               Index;
        ULONG                               Length;
        BOOL                                Result;

        Info = SetupDiGetClassDevs(&GUID_INTERFACE_XENIFACE, NULL, NULL,
                                   DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (Info == INVALID_HANDLE_VALUE) {
            DebugPrint(("XenIfaceItf: INVALID_HANDLE_VALUE\n"));
            return;
        }

        ItfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        for (Index = 0; 
            SetupDiEnumDeviceInterfaces(Info, NULL, &GUID_INTERFACE_XENIFACE, Index, &ItfData); 
            ++Index) {
            // query size needed
            SetupDiGetDeviceInterfaceDetail(Info, &ItfData, NULL, 0,
                                            &Length, NULL);
            ItfDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)new BYTE[Length];
            if (ItfDetailData == NULL) {
                DebugPrint(("XenIfaceItf: Failed to allocate %d bytes\n", Length));
                continue;
            }

            // query path
            ItfDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
            Result = SetupDiGetDeviceInterfaceDetail(Info, &ItfData,
                                            ItfDetailData, Length,
                                            NULL, NULL);
            // try to create
            if (Result) {
                DebugPrint(("XenIfaceItf: Creating \"%s\"\n", ItfDetailData->DevicePath));
                m_handle = CreateFile(ItfDetailData->DevicePath,
                                    GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL, OPEN_EXISTING, 0, NULL);
            }
            delete [] ItfDetailData;
            if (m_handle != INVALID_HANDLE_VALUE)
                break;

            // reset for next pass
            ItfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        }
        SetupDiDestroyDeviceInfoList(Info);
    }
    ~XenIfaceItf() 
    {
        if (m_handle != INVALID_HANDLE_VALUE)
            CloseHandle(m_handle);
    }

    String    Read(const String& path)
    {
        DWORD       bytes(0);
        BOOL        result;
        char*       out;

        if (m_handle == INVALID_HANDLE_VALUE)
            ThrowIfFailed(E_HANDLE);

        DebugPrint(("XenIfaceItf: Read \"%s\"\n", (const char*)path.c_str()));
        result = DeviceIoControl(m_handle, IOCTL_XENIFACE_STORE_READ,
                    (void*)path.c_str(), path.length() + 1,
                    NULL, 0, 
                    &bytes, NULL);
        ThrowIfUnexpectedError(result, GetLastError());
        if (bytes == 0)
            ThrowIfFailed(E_UNEXPECTED);
        DebugPrint(("XenIfaceItf: Read \"%s\" => %d bytes\n", (const char*)path.c_str(), bytes));

        out = new char[bytes];
        if (!out) 
            ThrowIfFailed(E_OUTOFMEMORY);

        result = DeviceIoControl(m_handle, IOCTL_XENIFACE_STORE_READ,
                    (void*)path.c_str(), path.length() + 1,
                    out, bytes,
                    &bytes, NULL);
        if (!result) {
            delete [] out;
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        } 
        String value = out;
        delete [] out;
        DebugPrint(("XenIfaceItf: Read \"%s\" => \"%s\"\n", (const char*)path.c_str(), (const char*)value.c_str()));
        return value;
    }
    void      Write(const String& path, const String& value) 
    {
        DWORD       bytes;
        BOOL        result;
        DWORD       insize = path.length() + 1 + value.length() + 1;
        char*       in;

        if (m_handle == INVALID_HANDLE_VALUE)
            ThrowIfFailed(E_HANDLE);

        in = new char[insize];
        if (!in)
            ThrowIfFailed(E_OUTOFMEMORY);

        memcpy(in, path.c_str(), path.length() + 1);
        memcpy(in + path.length() + 1, value.c_str(), value.length() + 1);

        DebugPrint(("XenIfaceItf: Write \"%s\" = \"%s\"\n", (const char*)path.c_str(), (const char*)value.c_str()));
        result = DeviceIoControl(m_handle, IOCTL_XENIFACE_STORE_WRITE,
                    (void*)in, insize, 
                    NULL, 0, 
                    &bytes, NULL);

        delete [] in;
        if (!result)
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
    void      Remove(const String& path) 
    {
        DWORD       bytes;
        BOOL        result;

        if (m_handle == INVALID_HANDLE_VALUE)
            ThrowIfFailed(E_HANDLE);

        DebugPrint(("XenIfaceItf: Remove \"%s\"\n", (const char*)path.c_str()));
        result = DeviceIoControl(m_handle, IOCTL_XENIFACE_STORE_REMOVE,
                    (void*)path.c_str(), path.length() + 1,
                    NULL, 0, 
                    &bytes, NULL);
        if (!result)
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
    StringVct Directory(const String& path) 
    {
        DWORD       bytes(0);
        BOOL        result;
        char*       out;

        if (m_handle == INVALID_HANDLE_VALUE)
            ThrowIfFailed(E_HANDLE);

        DebugPrint(("XenIfaceItf: Directory \"%s\"\n", (const char*)path.c_str()));
        result = DeviceIoControl(m_handle, IOCTL_XENIFACE_STORE_DIRECTORY,
                    (void*)path.c_str(), path.length() + 1,
                    NULL, 0, 
                    &bytes, NULL);
        ThrowIfUnexpectedError(result, GetLastError());
        if (bytes == 0)
            ThrowIfFailed(E_UNEXPECTED);
        DebugPrint(("XenIfaceItf: Directory \"%s\" => %d bytes\n", (const char*)path.c_str(), bytes));

        out = new char[bytes];
        if (!out) 
            ThrowIfFailed(E_OUTOFMEMORY);

        result = DeviceIoControl(m_handle, IOCTL_XENIFACE_STORE_DIRECTORY,
                    (void*)path.c_str(), path.length() + 1,
                    out, bytes,
                    &bytes, NULL);
        if (!result) {
            delete [] out;
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        } 
        StringVct values;
        for (const char* ptr = out; *ptr; ) {
            size_t len = strlen(ptr) + 1;
            String str = String(ptr);
            DebugPrint(("XenIfaceItf:  => \"%s\"\n", (const char*)str.c_str()));
            values.push_back(str);
            ptr += len;
        }
        delete [] out;
        return values;    
    }

private:
    HANDLE m_handle;

    void ThrowIfUnexpectedError(BOOL result, DWORD err)
    {
        if (result)
            return;

        // acceptable failure is STATUS_BUFFER_OVERFLOW, 1 of these should cover it
        if (err == ERROR_NOT_ENOUGH_MEMORY)
            return;
        if (err == ERROR_OUTOFMEMORY)
            return;
        if (err == ERROR_BUFFER_OVERFLOW)
            return;
        if (err == ERROR_INSUFFICIENT_BUFFER)
            return;
        if (err == ERROR_MORE_DATA)
            return;

        ThrowIfFailed(HRESULT_FROM_WIN32(err));
    }
    void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
            throw hr;
    }
};

#endif // _XENSTORE_INTERFACE_H_

