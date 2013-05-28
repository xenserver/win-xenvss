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
#include <vsprov.h>
#include <wbemidl.h>
#include <string>
using namespace std;

#include <stdio.h>

#include "debug.h"

static bool DebugLogToFile  = false;
static bool DebugLogToEventTrace = false;

static void WriteLogEntry(const char* str)
{
    FILE*   fp;

    if (fopen_s(&fp, "C:\\Program Files\\Citrix\\XenTools\\xenvss.log", "at") == 0) {
        fprintf(fp, str);
        fflush(fp);
        fclose(fp);
    }
}

#define BUFFER_SIZE 1024

const char* __HR(HRESULT hr)
{
    switch (hr) {
    case S_OK:              return "S_OK";
    case S_FALSE:           return "S_FALSE";
    case E_ACCESSDENIED:    return "E_ACCESSDENIED";
    case E_INVALIDARG:      return "E_INVALIDARG";
    case E_NOINTERFACE:     return "E_NOINTERFACE";
    case E_OUTOFMEMORY:     return "E_OUTOFMEMORY";
    case E_UNEXPECTED:      return "E_UNEXPECTED";
    case E_HANDLE:          return "E_HANDLE";

    case CLASS_E_CLASSNOTAVAILABLE: 
        return "CLASS_E_CLASSNOTAVAILABLE";
    case CLASS_E_NOAGGREGATION:
        return "CLASS_E_NOAGGREGATION";

    case RPC_E_TOO_LATE:
        return "RPC_E_TOO_LATE";
    case RPC_E_NO_GOOD_SECURITY_PACKAGES:
        return "RPC_E_NO_GOOD_SECURITY_PACKAGES";
    case RPC_E_ACCESS_DENIED:
        return "RPC_E_ACCESS_DENIED";

    case WBEM_E_FAILED:
        return "WBEM_E_FAILED";
    case WBEM_E_ACCESS_DENIED:
        return "WBEM_E_ACCESS_DENIED";
    case WBEM_E_INVALID_CLASS:
        return "WBEM_E_INVALID_CLASS";
    case WBEM_E_INVALID_PARAMETER:
        return "WBEM_E_INVALID_PARAMETER";
    case WBEM_E_OUT_OF_MEMORY:
        return "WBEM_E_OUT_OF_MEMORY";
    case WBEM_E_SHUTTING_DOWN:
        return "WBEM_E_SHUTTING_DOWN";
    case WBEM_E_TRANSPORT_FAILURE:
        return "WBEM_E_TRANSPORT_FAILURE";

    case VSS_E_BAD_STATE:
        return "VSS_E_BAD_STATE";
    case VSS_E_UNEXPECTED:
        return "VSS_E_UNEXPECTED";
    case VSS_E_PROVIDER_ALREADY_REGISTERED:
        return "VSS_E_PROVIDER_ALREADY_REGISTERED";
    case VSS_E_PROVIDER_NOT_REGISTERED:
        return "VSS_E_PROVIDER_NOT_REGISTERED";
    case VSS_E_PROVIDER_VETO:       
        return "VSS_E_PROVIDER_VETO";
    case VSS_E_PROVIDER_IN_USE:
        return "VSS_E_PROVIDER_IN_USE";
    case VSS_E_OBJECT_NOT_FOUND:
        return "VSS_E_OBJECT_NOT_FOUND";
    case VSS_S_ASYNC_PENDING:
        return "VSS_S_ASYNC_PENDING";
    case VSS_S_ASYNC_FINISHED:
        return "VSS_S_ASYNC_FINISHED";
    case VSS_S_ASYNC_CANCELLED:
        return "VSS_S_ASYNC_CANCELLED";
    case VSS_E_VOLUME_NOT_SUPPORTED:        
        return "VSS_E_VOLUME_NOT_SUPPORTED";
    case VSS_E_VOLUME_NOT_SUPPORTED_BY_PROVIDER:    
        return "VSS_E_VOLUME_NOT_SUPPORTED_BY_PROVIDER";
    case VSS_E_UNEXPECTED_PROVIDER_ERROR:   
        return "VSS_E_UNEXPECTED_PROVIDER_ERROR";

    default:                return "<unknown>";
    }
}
const char* __VssState(VSS_SNAPSHOT_STATE State)
{
    switch (State) {
    case VSS_SS_UNKNOWN:                return "UNKNOWN";
    case VSS_SS_PREPARING:              return "PREPARING";
    case VSS_SS_PREPARED:               return "PREPARED";
    case VSS_SS_PRECOMMITTED:           return "PRECOMMITTED";
    case VSS_SS_COMMITTED:              return "COMMITTED";
    case VSS_SS_PROCESSING_POSTCOMMIT:  return "PROCESSING_POSTCOMMIT";
    case VSS_SS_CREATED:                return "CREATED";
    default:                            return "<unknown>";
    }
}
const char* __BusType(VDS_STORAGE_BUS_TYPE type)
{
    switch (type) {
    case VDSBusTypeUnknown:     return "Unknown";
    case VDSBusTypeScsi:        return "Scsi";
    case VDSBusTypeAtapi:       return "Atapi";
    case VDSBusTypeAta:         return "Ata";
    case VDSBusType1394:        return "1394";
    case VDSBusTypeSsa:         return "Ssa";
    case VDSBusTypeFibre:       return "Fibre";
    case VDSBusTypeUsb:         return "Usb";
    case VDSBusTypeRAID:        return "RAID";
    case VDSBusTypeiScsi:       return "iScsi";
    case VDSBusTypeMaxReserved: return "MaxReserved";
    default:                    return "<UNKNOWN>";
    }
}
const char* __AddressType(VDS_INTERCONNECT_ADDRESS_TYPE type)
{
    switch (type) {
    case VDS_IA_UNKNOWN:    return "UNKNOWN";
    case VDS_IA_FCFS:       return "FCFS";
    case VDS_IA_FCPH:       return "FCPH";
    case VDS_IA_FCPH3:      return "FCPH3";
    case VDS_IA_MAC:        return "MAC";
    case VDS_IA_SCSI:       return "SCSI";
    default:                return "<UNKNOWN>";
    }
}
const char* __CodeSet(VDS_STORAGE_IDENTIFIER_CODE_SET codeset)
{
    switch (codeset) {
    case VDSStorageIdCodeSetReserved:   return "Reserved";
    case VDSStorageIdCodeSetBinary:     return "Binary";
    case VDSStorageIdCodeSetAscii:      return "Ascii";
    default:                            return "<UNKNOWN>";
    }
}
const char* __Type(VDS_STORAGE_IDENTIFIER_TYPE type)
{
    switch (type) {
    case VDSStorageIdTypeVendorSpecific:    return "VendorSpecific";
    case VDSStorageIdTypeVendorId:          return "VendorId";
    case VDSStorageIdTypeEUI64:             return "EUI64";
    case VDSStorageIdTypeFCPHName:          return "FCPHName";
    //case VDSStorageIdTypeSCSINameString:    return "SCSINameString";
    default:                                return "<UNKNOWN>";
    }
}

string __Guid(const GUID& guid)
{
    string retval('/0', 40);
    _snprintf_s(&retval[0], 40, 39, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], 
        guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return retval;
}
string __Bytes(PUCHAR Buffer, ULONG Length)
{
    string retval;
    for (ULONG i = 0; i < Length; ++i) {
        char buf[3] = { 0, 0, 0 };
        _snprintf_s(buf, sizeof(buf), 2, "%02x", Buffer[i]);
        retval += buf;
    }
    return retval;
}
string __String(PUCHAR Buffer, ULONG Length)
{
    string retval((const char*)Buffer, Length);
    return retval;
}

const char* __Prefix(const char* Prefix)
{
    if (Prefix)     
        return Prefix;
    return "";
}
void __DebugLun(const char* Func, const char* Prefix, const VDS_LUN_INFORMATION& Lun)
{
    __DebugMsg(Func, "%s Version                         : %d\n", __Prefix(Prefix), Lun.m_version);
    __DebugMsg(Func, "%s DeviceType                      : %d\n", __Prefix(Prefix), Lun.m_DeviceType);
    __DebugMsg(Func, "%s DeviceTypeModifier              : %d\n", __Prefix(Prefix), Lun.m_DeviceTypeModifier);
    __DebugMsg(Func, "%s CommandQueueing                 : %d\n", __Prefix(Prefix), Lun.m_bCommandQueueing);
    __DebugMsg(Func, "%s BusType                         : %s\n", __Prefix(Prefix), __BusType(Lun.m_BusType));
    __DebugMsg(Func, "%s VendorId                        : \"%s\"\n", __Prefix(Prefix), Lun.m_szVendorId);
    __DebugMsg(Func, "%s ProductId                       : \"%s\"\n", __Prefix(Prefix), Lun.m_szProductId);
    __DebugMsg(Func, "%s ProductRevision                 : \"%s\"\n", __Prefix(Prefix), Lun.m_szProductRevision);
    __DebugMsg(Func, "%s SerialNumber                    : \"%s\"\n", __Prefix(Prefix), Lun.m_szSerialNumber);
    __DebugMsg(Func, "%s DiskSignature                   : %s\n", __Prefix(Prefix), __Guid(Lun.m_diskSignature).c_str());
   
    __DebugMsg(Func, "%s DeviceId.Version                : %d\n", __Prefix(Prefix), Lun.m_deviceIdDescriptor.m_version);
    __DebugMsg(Func, "%s DeviceId.Identifiers            : %d\n", __Prefix(Prefix), Lun.m_deviceIdDescriptor.m_cIdentifiers);
    for (ULONG i = 0; i < Lun.m_deviceIdDescriptor.m_cIdentifiers; ++i) {
        __DebugMsg(Func, "%s DeviceId.Id[%d].CodeSet          : %s\n", __Prefix(Prefix), i, __CodeSet(Lun.m_deviceIdDescriptor.m_rgIdentifiers[i].m_CodeSet));
        __DebugMsg(Func, "%s DeviceId.Id[%d].Type             : %s\n", __Prefix(Prefix), i, __Type(Lun.m_deviceIdDescriptor.m_rgIdentifiers[i].m_Type));
        __DebugMsg(Func, "%s DeviceId.Id[%d].Identifier       : %d\n", __Prefix(Prefix), i, Lun.m_deviceIdDescriptor.m_rgIdentifiers[i].m_cbIdentifier);
        switch (Lun.m_deviceIdDescriptor.m_rgIdentifiers[i].m_CodeSet) {
        case VDSStorageIdCodeSetAscii:
            __DebugMsg(Func, "%s DeviceId.Id[%d].Identifier       : \"%s\"\n", __Prefix(Prefix), i, __String(Lun.m_deviceIdDescriptor.m_rgIdentifiers[i].m_rgbIdentifier, Lun.m_deviceIdDescriptor.m_rgIdentifiers[i].m_cbIdentifier).c_str());
            break;
        case VDSStorageIdCodeSetBinary:
        default:
            __DebugMsg(Func, "%s DeviceId.Id[%d].Identifier       : %s\n", __Prefix(Prefix), i, __Bytes(Lun.m_deviceIdDescriptor.m_rgIdentifiers[i].m_rgbIdentifier, Lun.m_deviceIdDescriptor.m_rgIdentifiers[i].m_cbIdentifier).c_str());
            break;
        }
    }

    __DebugMsg(Func, "%s Interconnects                   : %d\n", __Prefix(Prefix), Lun.m_cInterconnects);
    for (ULONG i = 0; i < Lun.m_cInterconnects; ++i) {
        __DebugMsg(Func, "%s Interconnect[%d].AddressType     : %s\n", __Prefix(Prefix), i, __AddressType(Lun.m_rgInterconnects[i].m_addressType));
        __DebugMsg(Func, "%s Interconnect[%d].Port            : %d\n", __Prefix(Prefix), i, Lun.m_rgInterconnects[i].m_cbPort);
        __DebugMsg(Func, "%s Interconnect[%d].Port            : %s\n", __Prefix(Prefix), i, __Bytes(Lun.m_rgInterconnects[i].m_pbPort, Lun.m_rgInterconnects[i].m_cbPort).c_str());
        __DebugMsg(Func, "%s Interconnect[%d].Address         : %d\n", __Prefix(Prefix), i, Lun.m_rgInterconnects[i].m_cbAddress);
        __DebugMsg(Func, "%s Interconnect[%d].Address         : %s\n", __Prefix(Prefix), i, __Bytes(Lun.m_rgInterconnects[i].m_pbAddress, Lun.m_rgInterconnects[i].m_cbAddress).c_str());
    }
}
void __DebugGuid(const char* Func, const GUID& guid, const char* Name)
{
    __DebugMsg(Func, "%s = {%s}\n", Name, __Guid(guid).c_str());
}
void __DebugMsg(const char* Prefix, const char* Format, ...)
{
    static char Buffer[BUFFER_SIZE];
    va_list     Args;
    string      Message(Prefix);

    va_start(Args, Format);
    vsnprintf_s(Buffer, sizeof(Buffer), BUFFER_SIZE-1, Format, Args);
    va_end(Args);

    Buffer[BUFFER_SIZE-1] = 0;

    Message += Buffer;
    OutputDebugStringA(Message.c_str());

#ifdef EVENT_TRACE
    if (DebugLogToEventTrace) {
        HANDLE      EventSrc;
        EventSrc = ::RegisterEventSource(NULL, L"XenVssProvider");
        if (EventSrc != NULL) {
            LPCSTR Strings[1] = { Message.c_str() };
            ::ReportEventA(EventSource, EVENTLOG_INFORMATION_TYPE, 0,
                         SIMHWPRV_EVENTLOG_INFO_GENERIC_MESSAGE,
                         NULL, 1, 0, 
                         &Strings[0], NULL);
            ::DeregisterEventSource(EventSrc);
        }
    }
#endif

    if (DebugLogToFile) {
        WriteLogEntry(Message.c_str());
    }
}
void
DebugInitializeLogging(
    )
{
    // read registry for settings
    HKEY    hKey;
    LONG    lResult;
    
    DebugLogToFile = false;
    DebugLogToEventTrace = false;

    lResult = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Citrix\\XenTools\\XenVss", 0, KEY_READ, &hKey);
    if (lResult == ERROR_SUCCESS) {
        DWORD   Type;
        DWORD   Data = 0;
        DWORD   DataSize = sizeof(DWORD);

        lResult = RegGetValueA(hKey, NULL, "LogToFile", 0, &Type, &Data, &DataSize);
        if (lResult == ERROR_SUCCESS) {
            if (Type == REG_DWORD && DataSize == sizeof(DWORD)) {
                if (Data) {
                    DebugLogToFile = true;
                }
            }
        }

        Data = 0;
        DataSize = sizeof(DWORD);

        lResult = RegGetValueA(hKey, NULL, "LogToEventTrace", 0, &Type, &Data, &DataSize);
        if (lResult == ERROR_SUCCESS) {
            if (Type == REG_DWORD && DataSize == sizeof(DWORD)) {
                if (Data) {
                    DebugLogToEventTrace = true;
                }
            }
        }

        RegCloseKey(hKey);
    }
}
