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

#include "provider.h"
#include "debug.h"
#include "bytes.h"

#include <winioctl.h>
#include <xeniface_interface.h>

#include <algorithm> 
#include <functional>
#include <cctype>
#include <locale>

// trim from start
static inline std::string &ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}

static inline bool __StringToBool(std::string& str) {
    for (std::string::iterator it = str.begin(); it != str.end(); ++it)
        *it = (char)tolower(*it);

    if (str == "false")
        return false;
    if (str == "0")
        return false;
    return true;
}

#define TRY(x)                  \
    try {                       \
        x;                      \
    } catch (...) {             \
    }

#pragma pack(push, 1)
typedef struct _VPD_IDENTIFICATION_DESCRIPTOR {
    UCHAR   CodeSet     : 4;
    UCHAR   Reserved    : 4;
    UCHAR   Type        : 4;
    UCHAR   Association : 2;
    UCHAR   Reserved2   : 2;
    UCHAR   Reserved3;
    UCHAR   Length;
    UCHAR   Identifier[1];
} VPD_IDENTIFICATION_DESCRIPTOR, *PVPD_IDENTIFICATION_DESCRIPTOR;
#pragma pack(pop)

typedef struct _SETWAIT_OP {
    const char*     set;
    const char*     pass;
    const char*     fail;
} SETWAIT_OP, *PSETWAIT_OP;

static const SETWAIT_OP CreateSnapshot      = { "create-snapshots", "snapshots-created", "snapshots-failed" };
static const SETWAIT_OP CreateSnapshotInfo  = { "create-snapshotinfo", "snapshotinfo-created", "snapshotinfo-failed"};
static const SETWAIT_OP ImportSnapshot      = { "import-snapshots", "snapshots-imported", "snapshot-import-failed" };
static const SETWAIT_OP DeportSnapshot      = { "deport-snapshots", "snapshots-deported", "deport-snapshots-failed" };
static const SETWAIT_OP DestroySnapshot     = { "destroy-snapshots", "snapshots-destroyed", "snapshots-destroy-failed" };

static __inline void 
__SetWait(
    XenIfaceItf&        itf,
    const string&       path,
    const SETWAIT_OP&   op
    )
{
    itf.Write(path + "/status", op.set);

    for (;;) {
        string value = itf.Read(path + "/status");

        if (value == op.pass) {
            TRY(itf.Remove(path + "/status"));
            return;
        }
        if (value == op.fail) {
            TRY(itf.Remove(path + "/status"));
            throw E_FAIL;
        }

        // check for invalid value
        if (value != op.set) {
            Trace("\"%s\" != \"%s\"|\"%s\" for \"%s\"\n", value.c_str(), op.pass, op.fail, op.set);
            throw E_INVALIDARG; // eek - unknown value
        }

        // wait a sec before continuing
        Sleep(1000);
    }
}
static __inline BOOLEAN
HasFlag(
    LONG        Flags,
    LONG        Mask
    )
{
    return (Flags & Mask) > 0 ? TRUE : FALSE;
}
static __inline string
Guid(
    const GUID& guid
    )
{
    char value[37];
    _snprintf_s(&value[0], 37, 36, "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], 
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    value[36] = 0;
    return string(value);
}
static __inline GUID
Guid(
    const string& str
    )
{
    DWORD v1,v2,v3,v4,v5,v6,v7,v8,v9,va,vb;
    sscanf_s(str.c_str(), "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        &v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8, &v9, &va, &vb);
    GUID value = { v1, (WORD)v2, (WORD)v3, { (BYTE)v4, (BYTE)v5, (BYTE)v6, (BYTE)v7, (BYTE)v8, (BYTE)v9, (BYTE)va, (BYTE)vb } };
    return value;
}
static __inline bool
__IsVdiUuid(
    const VDS_STORAGE_IDENTIFIER&   Id
    )
{
    return (Id.m_CodeSet == VDSStorageIdCodeSetAscii &&
            Id.m_Type == VDSStorageIdTypeVendorSpecific &&
            (Id.m_cbIdentifier == 36 || Id.m_cbIdentifier == 37));
}
static __inline bool
__IsTargetId(
    const VDS_STORAGE_IDENTIFIER&   Id
    )
{
    return (Id.m_CodeSet == VDSStorageIdCodeSetAscii &&
            Id.m_Type == VDSStorageIdTypeVendorSpecific &&
            Id.m_cbIdentifier == 4);
}
static __inline bool
GetVdi(
    const VDS_LUN_INFORMATION&  Lun, 
    GUID*                       Vdi
    ) 
{
    for (ULONG i = 0; i < Lun.m_deviceIdDescriptor.m_cIdentifiers; ++i) {
        const VDS_STORAGE_IDENTIFIER* Id = &Lun.m_deviceIdDescriptor.m_rgIdentifiers[i];
        if (__IsTargetId(*Id)) {
            if (Vdi) {
                XenIfaceItf Store;

                string targetid = trim(string((const char*)Id->m_rgbIdentifier, 4));
                try {
                    char    path[MAX_PATH];

                    _snprintf_s(path, sizeof(path), MAX_PATH-1,
                                "data/scsi/target/%s/frontend",
                                targetid.c_str());
                    string frontend = Store.Read(path);

                    _snprintf_s(path, sizeof(path), MAX_PATH-1,
                                "%s/backend",
                                frontend.c_str());
                    string backend = Store.Read(path);

                    _snprintf_s(path, sizeof(path), MAX_PATH-1,
                                "%s/sm-data/vdi-uuid",
                                backend.c_str());
                    string vdiuuid = Store.Read(path);

                    *Vdi = Guid(vdiuuid);
                } catch (...) {
                    Trace("Exception trying to find vdi-uuid for target %s\n", targetid.c_str());
                }
            }
            return true;
        }
    }
    for (ULONG i = 0; i < Lun.m_deviceIdDescriptor.m_cIdentifiers; ++i) {
        const VDS_STORAGE_IDENTIFIER* Id = &Lun.m_deviceIdDescriptor.m_rgIdentifiers[i];
        if (__IsVdiUuid(*Id)) {
            if (Vdi) {
                *Vdi = Guid(string((const char*)Id->m_rgbIdentifier, 36));
            }
            return true;
        }
    }
    return false;
}
static __inline void*
Clone(
    const void*                 Src,
    size_t                      Len
    )
{
    void* Dst;

    Dst = (void*)::CoTaskMemAlloc(Len);
    if (Dst) {
        if (Src) {
            ::CopyMemory(Dst, Src, Len);
        } else {
            ::ZeroMemory(Dst, Len);
        }
    } else {
        throw E_OUTOFMEMORY;
    }

    return Dst;
}
static __inline bool
__CloneFromPage83(
    VDS_STORAGE_IDENTIFIER&     DstId,
    const Bytes&                Page83
    )
{
    for (size_t i = 4; i < Page83.Length(); ) {
        const PVPD_IDENTIFICATION_DESCRIPTOR Desc = 
                    (const PVPD_IDENTIFICATION_DESCRIPTOR)Page83.Ptr(i);

        if (DstId.m_CodeSet == (VDS_STORAGE_IDENTIFIER_CODE_SET)Desc->CodeSet &&
            DstId.m_Type == (VDS_STORAGE_IDENTIFIER_TYPE)Desc->Type && 
            Desc->Association == 0) {

            DstId.m_cbIdentifier    = Desc->Length;
            DstId.m_rgbIdentifier   = (BYTE*)Clone(Desc->Identifier, Desc->Length);
            return true;
        }

        i += (Desc->Length + 4);
    }
    return false;
}
//=============================================================================
class AutoLock 
{
public:
    AutoLock(CRITICAL_SECTION& cs) : m_cs(&cs) 
    { EnterCriticalSection(m_cs); }
    
    ~AutoLock() 
    { Unlock(); }
    
    void Unlock() 
    { if (m_cs) LeaveCriticalSection(m_cs); m_cs = NULL; }
private:
    LPCRITICAL_SECTION m_cs;
};
//=============================================================================
XenVssProvider::XenVssProvider(
    ) : m_State(VSS_SS_UNKNOWN), m_SetId(GUID_NULL), m_Context(0), m_InVm(false), m_UseSrcSerialNumber(false), m_IsVssSupported(true)
{
    DebugInitializeLogging();

    Trace("====>\n");
    InitializeCriticalSection(&m_CritSec);

    XenIfaceItf Store;
    try {
        m_Vm = Store.Read("vss");
        TRY(Store.Remove(m_Vm + "/snapshot"));
        TRY(Store.Remove(m_Vm + "/snapinfo"));
        TRY(Store.Remove(m_Vm + "/snapuuid"));
        Store.Write(m_Vm + "/status", "provider-initialized");
        m_InVm = true;
    } catch (HRESULT hr) {
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        Trace("Exception UNKNOWN\n");
    }

    // set m_UseSrcSerialNumber to false if VBD uses StorageManager's Page80/Page83 data
    // set m_UseSrcSerialNumber to true if VBD uses hard-coded Page80/Page83 data

    Trace("<==== %s\n", m_InVm ? "IN-VM" : "ON-BAREMETAL");
}
XenVssProvider::~XenVssProvider(
    )
{
    Trace("====>\n");
    DeleteCriticalSection(&m_CritSec);
    Trace("<====\n");
}
HRESULT 
XenVssProvider::FinalConstruct(
    )
{
    return S_OK;
}
void
XenVssProvider::FinalRelease(
    )
{
}
//=============================================================================
// IVssHardwareSnapshotProvider
STDMETHODIMP 
XenVssProvider::AreLunsSupported(
    __in LONG                       Count, 
    __in LONG                       Context, 
    __in VSS_PWSZ*                  Devices, 
    __inout VDS_LUN_INFORMATION*    Luns, 
    __out BOOL*                     IsSupported
    )
{
    HRESULT     hr(S_OK);
    AutoLock    Lock(m_CritSec);

    Trace("====> (%d, %08x, 0x%p, 0x%p, 0x%p)\n", Count, Context, Devices, Luns, IsSupported);
    for (LONG Index = 0; Index < Count; ++Index) {
        Trace("Index %d = \"%ws\"\n", Index, Devices[Index]);
        TraceLun(NULL, Luns[Index]);
    }

    *IsSupported = (IsVSSSupported() ? TRUE : FALSE);
    try {
        if ( HasFlag(Context, VSS_VOLSNAP_ATTR_PLEX) &&
            !HasFlag(Context, VSS_VOLSNAP_ATTR_TRANSPORTABLE)) {
            Trace("Invalid Context (%08x) vetoed support\n", Context);
            *IsSupported = FALSE;
        }
        for (LONG Index = 0; Index < Count && *IsSupported; ++Index) {
            if (!GetVdi(Luns[Index], NULL)) {
                Trace("Device \"%ws\" vetoed support\n", Devices[Index]);
                *IsSupported = FALSE;
            }
        }
    } catch (HRESULT _hr) {
        hr = _hr;
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        hr = E_UNEXPECTED;
        Trace("Exception E_UNEXPECTED\n");
    }

    Trace("*IsSupported = %s\n", *IsSupported ? "TRUE" : "FALSE");
    TraceHR(hr);

    return hr;
}
STDMETHODIMP 
XenVssProvider::BeginPrepareSnapshot(   
    __in VSS_ID                     SetId, 
    __in VSS_ID                     SnapId, 
    __in LONG                       Context, 
    __in LONG                       Count, 
    __in VSS_PWSZ*                  Devices, 
    __inout VDS_LUN_INFORMATION*    Luns
    )
{
    HRESULT     hr(S_OK);
    AutoLock    Lock(m_CritSec);

    Trace("====> (..., ..., %08x, %d, 0x%p, 0x%p)\n", Context, Count, Devices, Luns);
    TraceGUID(SetId);
    TraceGUID(SnapId);
    for (LONG Index = 0; Index < Count; ++Index) {
        Trace("Index %d = \"%ws\"\n", Index, Devices[Index]);
        TraceLun(NULL, Luns[Index]);
    }

    try {
        if (!m_IsVssSupported) {
            Trace("VSS support VETOed\n");
            throw VSS_E_PROVIDER_VETO;
        }
        if (!IsRunningOnVM()) {
            Trace("Not Running on a VM\n");
            throw VSS_E_PROVIDER_VETO;
        }

        if ( HasFlag(Context, VSS_VOLSNAP_ATTR_PLEX) &&
            !HasFlag(Context, VSS_VOLSNAP_ATTR_TRANSPORTABLE)) {
            Trace("Invalid Context %08x\n", Context);
            throw VSS_E_UNSUPPORTED_CONTEXT;
        }

        switch (m_State) {
        case VSS_SS_UNKNOWN:    
            break;
        case VSS_SS_PREPARING:  
            if (!IsEqualGUID(SetId, m_SetId)) {
                Trace("Invalid SetId {%s}\n", Guid(SetId));
                throw VSS_E_PROVIDER_VETO;
            }
            break;
        default:
            Trace("Invalid State (%s)\n", __VssState(m_State));
            throw VSS_E_PROVIDER_VETO;
        }
        
        for (LONG Index = 0; Index < Count; ++Index) {
            GUID Vdi;
            if (GetVdi(Luns[Index], &Vdi)) {
                Trace("Adding VDI {%s}\n", Guid(Vdi).c_str());
                m_Snapshots[Vdi] = GUID_NULL;
            }
        }

        m_SetId = SetId;
        m_State = VSS_SS_PREPARING;
        m_Context = Context;
    } catch (HRESULT _hr) {
        AbortSnapshots(SetId);
        hr = _hr;
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        AbortSnapshots(SetId);
        hr = E_UNEXPECTED;
        Trace("Exception E_UNEXPECTED\n");
    }
    
    TraceHR(hr);

    return hr;
}
STDMETHODIMP 
XenVssProvider::FillInLunInfo(
    __in VSS_PWSZ                   Device, 
    __inout VDS_LUN_INFORMATION*    Lun, 
    __out BOOL*                     IsSupported
    )
{
    AutoLock    Lock(m_CritSec);

    Trace("====> (\"%ws\", 0x%p, 0x%p)\n", Device, Lun, IsSupported);
    TraceLun(NULL, *Lun);

    *IsSupported = FALSE;
    if (IsVSSSupported() && GetVdi(*Lun, NULL)) {
        *IsSupported = TRUE;
    }

    Trace("*IsSupported = %s\n", *IsSupported ? "TRUE" : "FALSE");
    TraceHR(S_OK);
    return S_OK;
}
STDMETHODIMP 
XenVssProvider::GetTargetLuns(
    __in LONG                       Count, 
    __in VSS_PWSZ*                  Devices, 
    __in VDS_LUN_INFORMATION*       SrcLuns, 
    __inout VDS_LUN_INFORMATION*    DstLuns
    )
{
    HRESULT     hr(S_OK);
    AutoLock    Lock(m_CritSec);

    Trace("====> (%d, 0x%p, 0x%p, 0x%p)\n", Count, Devices, SrcLuns, DstLuns);
    for (LONG Index = 0; Index < Count; ++Index) {
        Trace("Index %d = \"%ws\"\n", Index, Devices[Index]);
        TraceLun("Src", SrcLuns[Index]);
    }

    XenIfaceItf Store;
    try {
        if (m_State == VSS_SS_PROCESSING_POSTCOMMIT) {
            Store.Remove(m_Vm + "/snapshot");
            for (GUID_GUID_MAP::iterator it = m_Snapshots.begin(); it != m_Snapshots.end(); ++it) {
                if (!IsEqualGUID(it->second, GUID_NULL)) {
                    Store.Write(m_Vm + "/snapshot/" + Guid(it->second), "");
                }
            }
            __SetWait(Store, m_Vm, CreateSnapshotInfo);
        }

        string SnapInfo = Store.Read(m_Vm + "/snapinfo"); // appended to DeviceIdDescriptors

        for (LONG Index = 0; Index < Count; ++Index) {
            GUID Vdi;
            if (GetVdi(SrcLuns[Index], &Vdi)) {
                if (m_Snapshots.find(Vdi) != m_Snapshots.end()) {
                    CloneLunInfo(DstLuns[Index], SrcLuns[Index], SnapInfo, Vdi, m_Snapshots[Vdi]);
                }
            }
        }

        m_State = VSS_SS_CREATED;
    } catch (HRESULT _hr) {
        AbortSnapshots(GUID_NULL);
        hr = _hr;
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        AbortSnapshots(GUID_NULL);
        hr = E_UNEXPECTED;
        Trace("Exception E_UNEXPECTED\n");
    }    

    TraceHR(hr);
    return hr;
}
STDMETHODIMP 
XenVssProvider::LocateLuns(
    __in LONG                       Count, 
    __in VDS_LUN_INFORMATION*       Luns
    )
{
    HRESULT     hr(S_OK);
    AutoLock    Lock(m_CritSec);

    Trace("====> (%d, 0x%p)\n", Count, Luns);
    for (LONG Index = 0; Index < Count; ++Index) {
        TraceLun(NULL, Luns[Index]);
    }

    XenIfaceItf Store;
    try {
        Store.Remove(m_Vm + "/snapshot");

        LONG SnapCount = 0;
        for (LONG Index = 0; Index < Count; ++Index) {
            GUID Vdi;
            if (GetVdi(Luns[Index], &Vdi)) {
                Store.Write(m_Vm + "/snapshot/" + Guid(Vdi), "");
                ++SnapCount;
            }
        }
        if (SnapCount) {
            __SetWait(Store, m_Vm, ImportSnapshot);
        }
    } catch (HRESULT _hr) {
        hr = _hr;
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        hr = E_UNEXPECTED;
        Trace("Exception E_UNEXPECTED\n");
    }    
    TRY(Store.Remove(m_Vm + "/snapshot"));

    TraceHR(hr);

    return hr;
}
STDMETHODIMP 
XenVssProvider::OnLunEmpty(
    __in VSS_PWSZ                   Device, 
    __in VDS_LUN_INFORMATION*       Lun
    )
{
    HRESULT     hr(S_OK);
    AutoLock    Lock(m_CritSec);

    Trace("====> (\"%ws\", 0x%p)\n", Device, Lun);
    TraceLun(NULL, *Lun);

    XenIfaceItf Store;
    try {
        if (m_State != VSS_SS_UNKNOWN) {
            Trace("Invalid State (%s)\n", __VssState(m_State));
            throw VSS_E_PROVIDER_VETO;
        }

        GUID        Vdi;
        if (!GetVdi(*Lun, &Vdi)) {
            Trace("VDI Not Found\n");
            throw VSS_E_PROVIDER_VETO;
        }

        Store.Remove(m_Vm + "/snapshot");
        Store.Write(m_Vm + "/snapshot/" + Guid(Vdi), "");
        TRY(__SetWait(Store, m_Vm, DeportSnapshot));
        __SetWait(Store, m_Vm, DestroySnapshot);
    } catch (HRESULT _hr) {
        hr = _hr;
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        hr = E_UNEXPECTED;
        Trace("Exception E_UNEXPECTED\n");
    }
    TRY(Store.Remove(m_Vm + "/snapshot"));
    TRY(Store.Remove(m_Vm + "/snapinfo"));
    TRY(Store.Remove(m_Vm + "/snapuuid"));

    TraceHR(hr);

    return hr;
}
//=============================================================================
// IVssHardwareSnapshotProviderEx
STDMETHODIMP 
XenVssProvider::GetProviderCapabilities(
    __out ULONGLONG*                Caps
    )
{
    Trace("====> (0x%p)\n", Caps);

    TraceHR(S_OK);
    return S_OK;
}
STDMETHODIMP 
XenVssProvider::OnLunStateChange(
    __in VDS_LUN_INFORMATION *      SnapLuns, 
    __in VDS_LUN_INFORMATION *      OriginalLuns, 
    __in DWORD                      Count, 
    __in DWORD                      Flags
    )
{
    Trace("====> (0x%p, 0x%p, %d, %08x)\n", SnapLuns, OriginalLuns, Count, Flags);
    for (DWORD Index = 0; Index < Count; ++Index) {
        TraceLun("Snap", SnapLuns[Index]);
    }
    for (DWORD Index = 0; Index < Count; ++Index) {
        TraceLun("Orig", OriginalLuns[Index]);
    }

    TraceHR(S_OK);
    return S_OK;
}
STDMETHODIMP 
XenVssProvider::OnReuseLuns(
    __in VDS_LUN_INFORMATION *      SnapLuns, 
    __in VDS_LUN_INFORMATION *      OriginalLuns, 
    __in DWORD                      Count
    )
{
    Trace("====> (0x%p, 0x%p, %d)\n", SnapLuns, OriginalLuns, Count);
    for (DWORD Index = 0; Index < Count; ++Index) {
        TraceLun("Snap", SnapLuns[Index]);
    }
    for (DWORD Index = 0; Index < Count; ++Index) {
        TraceLun("Orig", OriginalLuns[Index]);
    }

    TraceHR(S_OK);
    return S_OK;
}
STDMETHODIMP 
XenVssProvider::ResyncLuns(
    __in VDS_LUN_INFORMATION *      SrcLuns, 
    __in VDS_LUN_INFORMATION *      DstLuns, 
    __in DWORD                      Count, 
    __out IVssAsync**               Async
    )
{
    Trace("====> (0x%p, 0x%p, %d, 0x%p)\n", SrcLuns, DstLuns, Count, Async);
    for (DWORD Index = 0; Index < Count; ++Index) {
        TraceLun("Src", SrcLuns[Index]);
    }
    for (DWORD Index = 0; Index < Count; ++Index) {
        TraceLun("Dst", DstLuns[Index]);
    }

    *Async = NULL;
    TraceHR(S_OK);
    return S_OK;
}
//=============================================================================
// IVssProviderCreateSnapshotSet
STDMETHODIMP 
XenVssProvider::EndPrepareSnapshots(
    __in VSS_ID                     SetId
    )
{
    HRESULT     hr(S_OK);
    AutoLock    Lock(m_CritSec);
    Trace("====> (...)\n");
    TraceGUID(SetId);

    try {
        if (!m_IsVssSupported) {
            Trace("VSS support VETOed\n");
            throw VSS_E_PROVIDER_VETO;
        }
        if (m_State != VSS_SS_PREPARING) {
            Trace("Invalid State (%s)\n", __VssState(m_State));
            throw VSS_E_PROVIDER_VETO;
        }
        if (!IsEqualGUID(SetId, m_SetId)) {
            Trace("Invalid SetId {%s}\n", Guid(SetId));
            throw VSS_E_PROVIDER_VETO;
        }
        m_State = VSS_SS_PREPARED;
    } catch (HRESULT _hr) {
        AbortSnapshots(SetId);
        hr = _hr;
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        AbortSnapshots(SetId);
        hr = E_UNEXPECTED;
        Trace("Exception E_UNEXPECTED\n");
    }

    TraceHR(hr);
    return hr;
}
STDMETHODIMP 
XenVssProvider::PreCommitSnapshots(
    __in VSS_ID                     SetId
    )
{
    HRESULT     hr(S_OK);
    AutoLock    Lock(m_CritSec);
    Trace("====> (...)\n");
    TraceGUID(SetId);

    try {
        if (!m_IsVssSupported) {
            Trace("VSS support VETOed\n");
            throw VSS_E_PROVIDER_VETO;
        }
        if (m_State != VSS_SS_PREPARED) {
            Trace("Invalid State (%s)\n", __VssState(m_State));
            throw VSS_E_PROVIDER_VETO;
        }
        if (!IsEqualGUID(SetId, m_SetId)) {
            Trace("Invalid SetId {%s}\n", Guid(SetId));
            throw VSS_E_PROVIDER_VETO;
        }
        m_State = VSS_SS_PRECOMMITTED;
    } catch (HRESULT _hr) {
        AbortSnapshots(SetId);
        hr = _hr;
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        AbortSnapshots(SetId);
        hr = E_UNEXPECTED;
        Trace("Exception E_UNEXPECTED\n");
    }

    TraceHR(hr);
    return hr;
}
STDMETHODIMP 
XenVssProvider::CommitSnapshots(
    __in VSS_ID                     SetId
    )
{
    HRESULT     hr(S_OK);
    AutoLock    Lock(m_CritSec);
    Trace("====> (...)\n");
    TraceGUID(SetId);

    XenIfaceItf Store;
    try {
        if (!m_IsVssSupported) {
            Trace("VSS support VETOed\n");
            throw VSS_E_PROVIDER_VETO;
        }
        if (m_State != VSS_SS_PRECOMMITTED) {
            Trace("Invalid State (%s)\n", __VssState(m_State));
            throw VSS_E_PROVIDER_VETO;
        }
        if (!IsEqualGUID(SetId, m_SetId)) {
            Trace("Invalid SetId {%s}\n", Guid(SetId));
            throw VSS_E_PROVIDER_VETO;
        }

        // send "create-snapshot" command
        Store.Remove(m_Vm + "/snapshot");
        for (GUID_GUID_MAP::iterator it = m_Snapshots.begin(); it != m_Snapshots.end(); ++it) {
            if (!IsEqualGUID(it->first, GUID_NULL)) {
                Store.Write(m_Vm + "/snapshot/" + Guid(it->first), "");
            }
        }
        __SetWait(Store, m_Vm, CreateSnapshot);

        // "create-snapshot" succeeded
        for (GUID_GUID_MAP::iterator it = m_Snapshots.begin(); it != m_Snapshots.end(); ++it) {
            string value = Store.Read(m_Vm + "/snapshot/" + Guid(it->first) + "/id");
            it->second = Guid(value);
        }

        m_State = VSS_SS_COMMITTED;
    } catch (HRESULT _hr) {
        AbortSnapshots(SetId);
        hr = _hr;
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        AbortSnapshots(SetId);
        hr = E_UNEXPECTED;
        Trace("Exception E_UNEXPECTED\n");
    }
    TRY(Store.Remove(m_Vm + "/snapshot"));

    TraceHR(hr);
    return hr;
}
STDMETHODIMP 
XenVssProvider::PostCommitSnapshots(
    __in VSS_ID                     SetId, 
    __in LONG                       Count
    )
{
    HRESULT     hr(S_OK);
    AutoLock    Lock(m_CritSec);
    Trace("====> (..., %d)\n", Count);
    TraceGUID(SetId);

    try {
        if (!m_IsVssSupported) {
            Trace("VSS support VETOed\n");
            throw VSS_E_PROVIDER_VETO;
        }
        if (m_State != VSS_SS_COMMITTED) {
            Trace("Invalid State (%s)\n", __VssState(m_State));
            throw VSS_E_PROVIDER_VETO;
        }
        if (!IsEqualGUID(SetId, m_SetId)) {
            Trace("Invalid SetId {%s}\n", Guid(SetId));
            throw VSS_E_PROVIDER_VETO;
        }
        m_State = VSS_SS_PROCESSING_POSTCOMMIT;
    } catch (HRESULT _hr) {
        AbortSnapshots(SetId);
        hr = _hr;
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        AbortSnapshots(SetId);
        hr = E_UNEXPECTED;
        Trace("Exception E_UNEXPECTED\n");
    }

    TraceHR(hr);
    return hr;
}
STDMETHODIMP 
XenVssProvider::PreFinalCommitSnapshots(
    __in VSS_ID                     SetId
    )
{
    HRESULT     hr(S_OK);
    AutoLock    Lock(m_CritSec);
    Trace("====> (...)\n");
    TraceGUID(SetId);

    try {
        if (!m_IsVssSupported) {
            Trace("VSS support VETOed\n");
            throw VSS_E_PROVIDER_VETO;
        }
        if (m_State != VSS_SS_CREATED) {
            Trace("Invalid State (%s)\n", __VssState(m_State));
            throw VSS_E_PROVIDER_VETO;
        }
        if (!IsEqualGUID(SetId, m_SetId)) {
            Trace("Invalid SetId {%s}\n", Guid(SetId));
            throw VSS_E_PROVIDER_VETO;
        }
        m_State = VSS_SS_CREATED;
    } catch (HRESULT _hr) {
        AbortSnapshots(SetId);
        hr = _hr;
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        AbortSnapshots(SetId);
        hr = E_UNEXPECTED;
        Trace("Exception E_UNEXPECTED\n");
    }

    TraceHR(hr);
    return hr;
}
STDMETHODIMP 
XenVssProvider::PostFinalCommitSnapshots(
    __in VSS_ID                     SetId
    )
{
    HRESULT     hr(S_OK);
    AutoLock    Lock(m_CritSec);
    Trace("====> (...)\n");
    TraceGUID(SetId);

    XenIfaceItf Store;
    try {
        if (!m_IsVssSupported) {
            Trace("VSS support VETOed\n");
            throw VSS_E_PROVIDER_VETO;
        }
        if (m_State != VSS_SS_CREATED) {
            Trace("Invalid State (%s)\n", __VssState(m_State));
            throw VSS_E_PROVIDER_VETO;
        }
        if (!IsEqualGUID(SetId, m_SetId)) {
            Trace("Invalid SetId {%s}\n", Guid(SetId));
            throw VSS_E_PROVIDER_VETO;
        }
        m_State = VSS_SS_UNKNOWN;
        m_SetId = GUID_NULL;
        m_Context = 0;
        m_Snapshots.clear();
    } catch (HRESULT _hr) {
        AbortSnapshots(SetId);
        hr = _hr;
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        AbortSnapshots(SetId);
        hr = E_UNEXPECTED;
        Trace("Exception E_UNEXPECTED\n");
    }
    TRY(Store.Remove(m_Vm + "/snapshot"));

    TraceHR(hr);
    return hr;
}
STDMETHODIMP 
XenVssProvider::AbortSnapshots(
    __in VSS_ID                     SetId
    )
{
    AutoLock    Lock(m_CritSec);
    ULONG       SnapshotCount(0);
    Trace("====> (...)\n");
    TraceGUID(SetId);

    XenIfaceItf Store;

    TRY(Store.Remove(m_Vm + "/snapshot"));
    for (GUID_GUID_MAP::iterator it = m_Snapshots.begin(); it != m_Snapshots.end(); ++it) {
        if (!IsEqualGUID(it->second, GUID_NULL)) {
            TRY(Store.Write(m_Vm + "/snapshot/" + Guid(it->second), ""));
            ++SnapshotCount;
        }
    }
    if (SnapshotCount) {
        TRY(__SetWait(Store, m_Vm, DeportSnapshot));
        TRY(__SetWait(Store, m_Vm, DestroySnapshot));
    }
    TRY(Store.Remove(m_Vm + "/snapshot"));
    TRY(Store.Remove(m_Vm + "/snapinfo"));
    TRY(Store.Remove(m_Vm + "/snapuuid"));

    m_Snapshots.clear();
    m_State = VSS_SS_UNKNOWN;
    m_SetId = GUID_NULL;
    m_Context = 0;
        
    TraceHR(S_OK);
    return S_OK;
}
//=============================================================================
// IVssProviderNotifications
STDMETHODIMP 
XenVssProvider::OnLoad(
    __in IUnknown*                  Unk
    )
{
    Trace("====> (0x%p)\n", Unk);
    TraceHR(S_OK);
    return S_OK;
}
STDMETHODIMP 
XenVssProvider::OnUnload(
    __in BOOL                       Force
    )
{
    Trace("====> (%s)\n", Force ? "TRUE" : "FALSE");
    TraceHR(S_OK);
    return S_OK;
}
//=============================================================================
bool 
XenVssProvider::IsRunningOnVM(
    )
{
    return m_InVm;
}
bool
XenVssProvider::IsVSSSupported(
    )
{
    XenIfaceItf Store;
    m_IsVssSupported = true;

    try {
        string value = Store.Read("vm-data/allowvssprovider");
        Trace("vm-data/allowvssprovider = %s\n", value.c_str());
        m_IsVssSupported = __StringToBool(value);
        if (!m_IsVssSupported) {
            // remove vss data, so agent doesnt use stale data
            TRY(Store.Remove(m_Vm + "/snapshot"));
            TRY(Store.Remove(m_Vm + "/snapinfo"));
            TRY(Store.Remove(m_Vm + "/snapuuid"));
        }
    } catch (HRESULT hr) {
        Trace("Exception %s:%08x\n", __HR(hr), hr);
    } catch (...) {
        Trace("Exception UNKNOWN\n");
    }

    Trace("%s\n", m_IsVssSupported ? "SUPPORTED" : "NOT_SUPPORTED");
    return m_IsVssSupported;
}
bool 
XenVssProvider::CloneLunInfo(
    VDS_LUN_INFORMATION&        Dst, 
    const VDS_LUN_INFORMATION&  Src,
    const string&               SnapInfo, // XML
    const GUID&                 SrcVdi,   // VDI of current disk
    const GUID&                 DstVdi)   // VDI of snapshot disk
{
    XenIfaceItf Store;
    try {
        ULONG  i;
        ULONG  Identifiers;

        Bytes  Page80;
        string Page80Base64;
        string SerialNumber;

        Page80Base64 = Store.Read(m_Vm + "/snapshot/" + Guid(DstVdi) + "/scsi/0x12/0x80");
        Page80.FromBase64(Page80Base64);
        SerialNumber.assign((const char*)Page80.Ptr(4), Page80.Length() - 4);
        rtrim(SerialNumber);

        Bytes  Page83;
        string Page83Base64;

        Page83Base64 = Store.Read(m_Vm + "/snapshot/" + Guid(DstVdi) + "/scsi/0x12/0x83");
        Page83.FromBase64(Page83Base64);

        Dst.m_version            = VER_VDS_LUN_INFORMATION;
        Dst.m_DeviceType         = Src.m_DeviceType;
        Dst.m_DeviceTypeModifier = Src.m_DeviceTypeModifier;
        Dst.m_bCommandQueueing   = Src.m_bCommandQueueing;
        Dst.m_BusType            = Src.m_BusType;
        Dst.m_szVendorId         = (char*)Clone(Src.m_szVendorId, strlen(Src.m_szVendorId) + 1);
        Dst.m_szProductId        = (char*)Clone(Src.m_szProductId, strlen(Src.m_szProductId) + 1);
        Dst.m_szProductRevision  = (char*)Clone(Src.m_szProductRevision, strlen(Src.m_szProductRevision) + 1);
        if (m_UseSrcSerialNumber)
            Dst.m_szSerialNumber = (char*)Clone(Src.m_szSerialNumber, strlen(Src.m_szSerialNumber) + 1);
        else
            Dst.m_szSerialNumber = (char*)Clone(SerialNumber.c_str(), SerialNumber.length() + 1);
        Dst.m_diskSignature      = DstVdi;
        Dst.m_cInterconnects     = 0;
        Dst.m_rgInterconnects    = NULL;

        Identifiers = Src.m_deviceIdDescriptor.m_cIdentifiers;

        Dst.m_deviceIdDescriptor.m_version       = Src.m_deviceIdDescriptor.m_version;
        Dst.m_deviceIdDescriptor.m_cIdentifiers  = Src.m_deviceIdDescriptor.m_cIdentifiers + 1;
        Dst.m_deviceIdDescriptor.m_rgIdentifiers = (VDS_STORAGE_IDENTIFIER*)Clone(NULL, sizeof(VDS_STORAGE_IDENTIFIER) * (Identifiers + 1));
        
        for (i = 0; i < Identifiers; ++i) {
            const VDS_STORAGE_IDENTIFIER& SrcId = Src.m_deviceIdDescriptor.m_rgIdentifiers[i];
                  VDS_STORAGE_IDENTIFIER& DstId = Dst.m_deviceIdDescriptor.m_rgIdentifiers[i];

            DstId.m_CodeSet = SrcId.m_CodeSet;
            DstId.m_Type    = SrcId.m_Type;

            if (__IsVdiUuid(SrcId)) {
                string Uuid = Guid(DstVdi);
                DstId.m_cbIdentifier = Uuid.length();
                DstId.m_rgbIdentifier = (BYTE*)Clone(&Uuid[0], Uuid.length());
            } else if (!__CloneFromPage83(DstId, Page83)) {
                // failed to clone from Page83Data, clone from Src as fallback
                DstId.m_cbIdentifier = SrcId.m_cbIdentifier;
                DstId.m_rgbIdentifier = (BYTE*)Clone(SrcId.m_rgbIdentifier, SrcId.m_cbIdentifier);
            }
        }

        Dst.m_deviceIdDescriptor.m_rgIdentifiers[Identifiers].m_CodeSet = VDSStorageIdCodeSetAscii;
        Dst.m_deviceIdDescriptor.m_rgIdentifiers[Identifiers].m_Type    = (enum _VDS_STORAGE_IDENTIFIER_TYPE)10;
        Dst.m_deviceIdDescriptor.m_rgIdentifiers[Identifiers].m_cbIdentifier = SnapInfo.length();
        Dst.m_deviceIdDescriptor.m_rgIdentifiers[Identifiers].m_rgbIdentifier= (BYTE*)Clone(&SnapInfo[0], SnapInfo.length());

        TraceLun("Src", Src);
        TraceLun("Dst", Dst);
        return true;
    } catch (HRESULT hr) {
        Trace("Exception %s:%08x\n", __HR(hr), hr);
        return false;
    } catch (...) {
        Trace("Exception E_UNEXPECTED\n");
        return false;
    }
}

