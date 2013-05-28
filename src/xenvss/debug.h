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

#ifndef _XENVSS_DEBUG_H_
#define _XENVSS_DEBUG_H_

#include <includes.h>
#include <stdio.h>
#include <string>
using namespace std;

extern const char* 
__HR(
    HRESULT hr
    );
extern const char*
__VssState(
    VSS_SNAPSHOT_STATE  State
    );
extern const char* 
__BusType(
    VDS_STORAGE_BUS_TYPE type
    );
extern const char* 
__AddressType(
    VDS_INTERCONNECT_ADDRESS_TYPE type
    );
extern const char* 
__CodeSet(
    VDS_STORAGE_IDENTIFIER_CODE_SET codeset
    );
extern const char* 
__Type(
    VDS_STORAGE_IDENTIFIER_TYPE type
    );

extern string 
__Guid(
    const GUID&               guid
    );
extern string 
__Bytes(
    PUCHAR                  Buffer, 
    ULONG                   Length
    );
extern string 
__String(
    PUCHAR                  Buffer,          
    ULONG                   Length
    );

extern void 
__DebugLun(
    const char*                 Function, 
    const char*                 Prefix, 
    const VDS_LUN_INFORMATION&  Lun
    );
extern void 
__DebugGuid(
    const char* Func, 
    const GUID& guid, 
    const char* Name
    );
extern void 
__DebugMsg(
    const char* Func, 
    const char* Format, 
    ...
    );
extern void
DebugInitializeLogging(
    );

//#ifdef _DEBUG

#define Trace(...)        \
        __DebugMsg("XENVSS|" __FUNCTION__ ": ", __VA_ARGS__)

#define TraceHR(hr)       \
        __DebugMsg("XENVSS|" __FUNCTION__ ": ", "<==== %s (%08x)\n", __HR(hr), hr)

#define TraceBool(b)      \
        __DebugMsg("XENVSS|" __FUNCTION__ ": ", "<==== %s\n", b ? "true" : "false")

#define TraceGUID(guid)   \
        __DebugGuid("XENVSS|" __FUNCTION__ ": ", guid, #guid)

#define TraceLun(prefix, lun) \
        __DebugLun("XENVSS|" __FUNCTION__ ": ", prefix, lun)

#define TraceIfFailed(hr, msg)  \
        if (FAILED(hr))         \
            Trace("%s failed with %s (%08x)\n", msg, __HR(hr), hr);

//#else // _DEBUG
//
//#define Trace(...)
//
//#define TraceHR(hr)
//
//#define TraceBool(b)
//
//#define TraceGUID(guid)
//
//#define TraceLun(prefix, lun)
//
//#define TraceIfFailed(hr, msg)
//
//#endif // _DEBUG

#endif // _XENVSS_DEBUG_H_


