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

#ifndef _XENVSS_PROVIDER_H_
#define _XENVSS_PROVIDER_H_

#include <includes.h>
#include "Resource.h"
#include "xenvss_i.h"

#include <map>
#include <string>
using namespace std;

inline bool operator<(const GUID& a, const GUID& b)
{
    return memcmp(&a, &b, sizeof(GUID)) < 0;
}
typedef map<GUID, GUID>     GUID_GUID_MAP;

class ATL_NO_VTABLE XenVssProvider :
        public CComObjectRootEx< CComSingleThreadModel >,
        public CComCoClass< XenVssProvider, &CLSID_XenVssProvider >,
        public IVssHardwareSnapshotProviderEx,
        public IVssProviderCreateSnapshotSet,
        public IVssProviderNotifications
{
public:
    XenVssProvider();
    ~XenVssProvider();

    DECLARE_REGISTRY_RESOURCEID(IDR_XENVSS_PROVIDER)
    DECLARE_NOT_AGGREGATABLE(XenVssProvider)

    BEGIN_COM_MAP(XenVssProvider)
        COM_INTERFACE_ENTRY(IVssHardwareSnapshotProvider)
        //COM_INTERFACE_ENTRY(IVssHardwareSnapshotProviderEx)
        COM_INTERFACE_ENTRY(IVssProviderCreateSnapshotSet)
        COM_INTERFACE_ENTRY(IVssProviderNotifications)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct();
	void FinalRelease();

public:
    // IVssHardwareSnapshotProvider
    STDMETHODIMP AreLunsSupported(
            __in LONG Count, 
            __in LONG Context, 
            __in VSS_PWSZ* Devices, 
            __inout VDS_LUN_INFORMATION* Luns, 
            __out BOOL* IsSupported);
    STDMETHODIMP BeginPrepareSnapshot(
            __in VSS_ID SetId, 
            __in VSS_ID SnapId, 
            __in LONG Context, 
            __in LONG Count, 
            __in VSS_PWSZ* Devices, 
            __inout VDS_LUN_INFORMATION* Luns);
    STDMETHODIMP FillInLunInfo(
            __in VSS_PWSZ Device, 
            __inout VDS_LUN_INFORMATION* Lun, 
            __out BOOL* IsSupported);
    STDMETHODIMP GetTargetLuns(
            __in LONG Count, 
            __in VSS_PWSZ* Devices, 
            __in VDS_LUN_INFORMATION* SrcLuns, 
            __inout VDS_LUN_INFORMATION* DstLuns);
    STDMETHODIMP LocateLuns(
            __in LONG Count, 
            __in VDS_LUN_INFORMATION* Luns);
    STDMETHODIMP OnLunEmpty(
            __in VSS_PWSZ Device, 
            __in VDS_LUN_INFORMATION* Lun);

public:
    // IVssHardwareSnapshotProviderEx
    STDMETHODIMP GetProviderCapabilities(
            __out ULONGLONG* Caps);
    STDMETHODIMP OnLunStateChange(
            __in VDS_LUN_INFORMATION *SnapLuns, 
            __in VDS_LUN_INFORMATION *OriginalLuns, 
            __in DWORD Count, 
            __in DWORD Flags);
    STDMETHODIMP OnReuseLuns(
            __in VDS_LUN_INFORMATION *SnapLuns, 
            __in VDS_LUN_INFORMATION *OriginalLuns, 
            __in DWORD Count);
    STDMETHODIMP ResyncLuns(
            __in VDS_LUN_INFORMATION *SrcLuns, 
            __in VDS_LUN_INFORMATION *DstLuns, 
            __in DWORD Count, 
            __out IVssAsync** Async);

public:
    // IVssProviderCreateSnapshotSet
    STDMETHODIMP EndPrepareSnapshots(
            __in VSS_ID SetId);
    STDMETHODIMP PreCommitSnapshots(
            __in VSS_ID SetId);
    STDMETHODIMP CommitSnapshots(
            __in VSS_ID SetId);
    STDMETHODIMP PostCommitSnapshots(
            __in VSS_ID SetId, 
            __in LONG Count);
    STDMETHODIMP PreFinalCommitSnapshots(
            __in VSS_ID SetId);
    STDMETHODIMP PostFinalCommitSnapshots(
            __in VSS_ID SetId);
    STDMETHODIMP AbortSnapshots(
            __in VSS_ID SetId);

public:
    // IVssProviderNotifications
    STDMETHODIMP OnLoad(
            __in IUnknown* Unk);
    STDMETHODIMP OnUnload(
            __in BOOL Force);

private:
    CRITICAL_SECTION        m_CritSec;

    VSS_SNAPSHOT_STATE      m_State;
    VSS_ID                  m_SetId;
    ULONG                   m_Context;
    string                  m_Vm;
    GUID_GUID_MAP           m_Snapshots;
    bool                    m_InVm;
    bool                    m_UseSrcSerialNumber;
    bool                    m_IsVssSupported;
    
private:
    bool IsRunningOnVM();
    bool IsVSSSupported();
    bool CloneLunInfo(
            VDS_LUN_INFORMATION&        Dst, 
            const VDS_LUN_INFORMATION&  Src,
            const string&               SnapInfo, // XML
            const GUID&                 SrcVdi,   // VDI of current disk
            const GUID&                 DstVdi);  // VDI of snapshot disk
};

OBJECT_ENTRY_AUTO(CLSID_XenVssProvider, XenVssProvider)

#endif // _XENVSS_PROVIDER_H_
