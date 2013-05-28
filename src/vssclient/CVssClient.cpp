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
#include <process.h>
#include <iostream>

#include "CVssClient.hpp"
#include "debug.h"
#include "atlbase.h"


#define DBGFUNC()       DBGPRINT(("\n" __FUNCTION__))
#define CHECK_COM(x)    this->ThrowIfComError(x, #x, __FUNCTION__)
#define CHECK_NULL(x)   this->ThrowIfNull(x, #x, __FUNCTION__)

typedef HRESULT (STDAPICALLTYPE *CREATE_VSS_BACKUP_COMPONENTS) (IVssBackupComponents **);

static const GUID GUID_PROV_XEN = {0x3aeb8223, 0xa8eb, 0x43a2, { 0x8f, 0xf7, 0x86, 0x83, 0x12, 0xe6, 0x7a, 0x8f }}; // {3AEB8223-A8EB-43a2-8FF7-868312E67A8F}
static const GUID GUID_NULL     = {0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};



HANDLE CVssClient::m_threadHandle = NULL;

// Converts a wstring into a GUID
inline GUID & WString2Guid(wstring src)
{
    // Check if this is a GUID
    static GUID result;
    HRESULT hr = ::CLSIDFromString(W2OLE(const_cast<WCHAR*>(src.c_str())), &result);
    if (FAILED(hr))
    {
        throw(E_INVALIDARG);
    }

    return result;
}


/******************************************************************************
 *                              Constructor()
 ******************************************************************************/
CVssClient::CVssClient()
{
    DBGFUNC();
    
    m_pVssObject            = NULL;
    m_bCoInitializeCalled   = false;
    m_errorCode             = 0;
    m_snapshotType            = SNAPSHOT_TYPE_VM;
}


/******************************************************************************
 *                              Destructor()
 ******************************************************************************/
CVssClient::~CVssClient()
{
    DBGFUNC();
    
    if (m_pVssObject)
        m_pVssObject->Release();

    if (m_bCoInitializeCalled)
        CoUninitialize();
    
}


/******************************************************************************
 *                              ThrowIfComError()
 ******************************************************************************/
void CVssClient::ThrowIfComError(HRESULT code, const char *msg, const char *func)
{
    m_errorCode = code;

    if (FAILED(m_errorCode)) 
    { 
        std::ostringstream stream;
        
        stream << msg << "@" << func << " failed";  
        m_errorMessage = stream.str();
        throw m_errorCode;
    }
}


/******************************************************************************
 *                              ThrowIfNull()
 ******************************************************************************/
void CVssClient::ThrowIfNull(const void *ptr, const char *msg, const char *func)
{
    if (ptr == NULL)
    {
        m_errorCode = VSS_E_OBJECT_NOT_FOUND;
        
        std::ostringstream stream;
        
        stream << msg << "@" << func << " is null";
        m_errorMessage = stream.str();
        throw m_errorCode; 
    }
}


/******************************************************************************
 *                              ThrowError()
 ******************************************************************************/
void CVssClient::ThrowError(int code, const char *msg)
{
    m_errorCode = code;
    m_errorMessage = msg;

    throw m_errorCode;
}
static HMODULE m_hinstLib = NULL;

/******************************************************************************
 *                              Initialize()
 ******************************************************************************/
void CVssClient::Initialize()
{
    HRESULT hr;

    
    DBGFUNC();

    m_errorState = XEN_VSS_REQ_ERROR_INIT_FAILED;
    
    // Initializing as a single threaded COM apartment as we anyways
    // do not use multiple threads here.
    CHECK_COM(CoInitializeEx(0, COINIT_APARTMENTTHREADED));
    
    m_bCoInitializeCalled = true;
    
    hr = CoInitializeSecurity (NULL,     
                               -1,                          // COM negotiates service                  
                               NULL,                        // Authentication services
                               NULL,                        // Reserved
                               RPC_C_AUTHN_LEVEL_PKT_PRIVACY,   // authentication
                               RPC_C_IMP_LEVEL_IDENTIFY, // Impersonation
                               NULL,                        // Authentication info 
                               EOAC_NONE,                   // Additional capabilities
                               NULL);                       // Reserved

    if (FAILED(hr) && (hr != RPC_E_TOO_LATE))
        this->ThrowError(hr, "CoInitializeSecurity() failed");
    
    if (!m_hinstLib) {
        m_hinstLib = LoadLibrary("vssapi.dll");
        if (m_hinstLib == NULL)
            this->ThrowError (GetLastError(), "Could not load vssapi.dll");
    }

   
}

void  CVssClient::InitVssObject() {


     CREATE_VSS_BACKUP_COMPONENTS    vssProc;
    
     vssProc = (CREATE_VSS_BACKUP_COMPONENTS)GetProcAddress(m_hinstLib, 
                                                           "?CreateVssBackupComponents@@YGJPAPAVIVssBackupComponents@@@Z");
    if (vssProc == NULL)
    {
        vssProc = (CREATE_VSS_BACKUP_COMPONENTS)GetProcAddress(m_hinstLib,
                                                               "?CreateVssBackupComponents@@YAJPEAPEAVIVssBackupComponents@@@Z");
    }
    
    if (vssProc == NULL)
        this->ThrowError (GetLastError(), "Could not find CreateVssBackupComponents()");
    
    CHECK_COM((vssProc)(&m_pVssObject));
    CHECK_COM(m_pVssObject->InitializeForBackup());
    if(m_snapshotType == SNAPSHOT_TYPE_VOLUME)
        CHECK_COM(m_pVssObject->SetContext (VSS_VOLSNAP_ATTR_TRANSPORTABLE | VSS_CTX_APP_ROLLBACK));            // just a volume snapshot. 
    else    
        CHECK_COM(m_pVssObject->SetContext (VSS_VOLSNAP_ATTR_TRANSPORTABLE |
                                        VSS_VOLSNAP_ATTR_PLEX | VSS_CTX_APP_ROLLBACK));            // we use this flag to tell xen provider that
                                                                                                   // that the snapshot needs to be made writeable.
    
    // non-component mode - backing up all files and components on volumes
    // saving bootable state
    // partial file backup is not support
    
    CHECK_COM(m_pVssObject->SetBackupState(true, true, VSS_BT_FULL, false));
}

void  CVssClient::ReleaseVssObject() {
    try {
        m_pVssObject->Release();
    }
    catch(...) {
        DBGPRINT(("Failed to release VSS object"));
    }
    m_pVssObject = NULL;
}

/******************************************************************************
 *                      WaitAndCheckForAsyncOperation
 ******************************************************************************/
HRESULT CVssClient::WaitAndCheckForAsyncOperation(IVssAsync* pAsync)
{
    HRESULT     hr;
    HRESULT     hrReturned;
    
    
    DBGFUNC();
    
    if (pAsync == NULL)
        return VSS_E_OBJECT_NOT_FOUND;
        
    hr = pAsync->Wait();
    if (FAILED(hr)) {
        DBGPRINT(("IVssAsync.Wait() failed %x", hr));
        return hr;
    }

    hr = pAsync->QueryStatus(&hrReturned, NULL);
    if (FAILED(hr)) {
        DBGPRINT(("IVssAsnyc.QueryStatus failed %x\n", hr));
        return hr;
    }
  
    if (hrReturned == VSS_S_ASYNC_PENDING) {
        DBGPRINT(("Async op still pending"));
    }

    return hrReturned;
}


/******************************************************************************
 *                              FindXenProvider()
 ******************************************************************************/
void CVssClient::FindXenProvider(void)
{
    HRESULT             hr;
    VSS_OBJECT_PROP     prop;
    VSS_PROVIDER_PROP   &prov       = prop.Obj.Prov;
    IVssEnumObject      *pVssEnum   = NULL;
    bool                found       = false;


    DBGFUNC();
    
    try
    {
        m_errorState = XEN_VSS_REQ_ERROR_PROV_NOT_LOADED;

        InitVssObject();

        CHECK_NULL(m_pVssObject);
        
        CHECK_COM(m_pVssObject->Query(GUID_NULL, VSS_OBJECT_NONE, VSS_OBJECT_PROVIDER, &pVssEnum));
        
        while (true)
        {
            ULONG   fetched = 0;
            
            hr = pVssEnum->Next(1, &prop, &fetched);
            if (FAILED(hr) || (fetched == 0))
            {
                DBGPRINT(("IVssEnumObject->Next %x %d\n", hr, fetched));
                break;
            }
            
            DBGPRINT(("%S %S type=%d\n", prov.m_pwszProviderName, prov.m_pwszProviderVersion, prov.m_eProviderType));
            
            if ((prov.m_eProviderType == VSS_PROV_HARDWARE) && (prov.m_ProviderId == GUID_PROV_XEN))
            {
               found = true;
               break;
            }
        }
        
        if (found == false)
            this->ThrowError (VSS_E_PROVIDER_NOT_REGISTERED, "Could not find Xen VSS HW Provider");     
                   
    }
    catch (...)
    {
        SAFE_RELEASE(pVssEnum);
        throw;
    }
    
    SAFE_RELEASE(pVssEnum);
    ReleaseVssObject();

}


/******************************************************************************
 *                              FindXenVolumes()
 ******************************************************************************/
void CVssClient::FindXenVolumes(vector <wstring> &volumes)
{
    HANDLE          volumeHdl = INVALID_HANDLE_VALUE;
    WCHAR           volumeName[MAX_PATH];
    vector<wstring> volumeList;
    

    DBGFUNC();

    m_errorState = XEN_VSS_REQ_ERROR_NO_VOLUMES_SUPPORTED;    
    
    CHECK_NULL(m_pVssObject);
    
    volumes.clear();
        
    volumeHdl = FindFirstVolumeW(volumeName, MAX_PATH);
    
    if (volumeHdl != INVALID_HANDLE_VALUE)
    {
        do 
        {
            BOOL        supported;
            HRESULT     hr;
            
            hr = m_pVssObject->IsVolumeSupported(GUID_PROV_XEN, volumeName, &supported);
            if (SUCCEEDED(hr) && supported)
            {
                volumes.push_back(volumeName);
            }
            
            DBGPRINT(("FindXenVolume %S %d\n", volumeName, supported));
        
        } while (FindNextVolumeW(volumeHdl, volumeName, MAX_PATH));

        FindVolumeClose(volumeHdl);
    }
    
    if (volumes.empty() == true)
        this->ThrowError (VSS_E_VOLUME_NOT_SUPPORTED_BY_PROVIDER, "Could not find Xen volumes");
}


/******************************************************************************
 *                           CreateSnapshotSet()
 ******************************************************************************/
void CVssClient::CreateSnapshotSet(SAVEBACKUPDOC_CALLBACK callback)
{
    HRESULT                             hr;
    IVssAsync                           *pAsync = NULL;
    vector<wstring>::const_iterator     iter;
    bool bWriterComponentsAdded = false, bBackupSucceeded = false, bSetBackupSucceededStatus = false;    
    BSTR bstrXml = NULL; 
    DBGFUNC();

    try
    {
        InitVssObject(); 

        m_errorState = XEN_VSS_REQ_ERROR_START_SNAPSHOT_SET_FAILED; 
        CHECK_NULL(m_pVssObject);
        
        // Start the shadow set
        
        CHECK_COM(m_pVssObject->StartSnapshotSet(&m_snapshotSetId));

        // Add the specified volumes to the shadow set

        m_errorState = XEN_VSS_REQ_ERROR_ADDING_VOLUME_TO_SNAPSET_FAILED;         
        if (m_volumesList.empty()) {
            DBGPRINT(("no volumes to add\n"));
            ThrowError(0, "No Volumes to Add\n");
        }
        for (iter=m_volumesList.begin(); iter!=m_volumesList.end(); iter++)
        {
            VSS_ID id;
            
            DBGPRINT(("adding vol = %S\n", iter->c_str()));
            
            CHECK_COM(m_pVssObject->AddToSnapshotSet((LPWSTR)iter->c_str(), GUID_PROV_XEN, &id));
            
            this->m_snapshotIds.push_back(id);
        }
        
        // Now add writer components to the backup so that the correct writers can be quiesced.
        bWriterComponentsAdded = AddWriterComponents();        
        
        // Prepare for backup. 

        m_errorState = XEN_VSS_REQ_ERROR_PREPARING_WRITERS; 
        CHECK_COM(m_pVssObject->PrepareForBackup(&pAsync));
        CHECK_COM(this->WaitAndCheckForAsyncOperation(pAsync));
        SAFE_RELEASE(pAsync);
        
        // Creates the shadow set 

        m_errorState = XEN_VSS_REQ_ERROR_CREATING_SNAPSHOT;
        CHECK_COM(m_pVssObject->DoSnapshotSet(&pAsync));
        CHECK_COM(this->WaitAndCheckForAsyncOperation(pAsync));
        SAFE_RELEASE(pAsync);
                
        // Now set the backup status for the writer components added to the backup.
        bBackupSucceeded = true;
        if(bWriterComponentsAdded)
        {
            SetWriterComponentsBackupSucceeded(true);
            bSetBackupSucceededStatus = true;
        }

        // Get the XML transportable ID
        m_errorState = XEN_VSS_REQ_ERROR_CREATING_SNAPSHOT_XML_STRING;
        CHECK_COM(m_pVssObject->SaveAsXML(&bstrXml));

        // Got a transportable ID, call the callback and free the string
        try {
            if (!callback((char *)bstrXml)) {
                this->ThrowError(0,"Could not process Backup Component Document");
            }
            if (bstrXml)
                ::SysFreeString(bstrXml);
        } catch (...) {
            if (bstrXml)
                ::SysFreeString(bstrXml);
            m_errorState = XEN_VSS_REQ_ERROR_CREATING_SNAPSHOT_XML_STRING;
            throw;
        }
    }
    catch (...)
    {
        // Check if writer components need to be informed about the backup completion status
        if(bWriterComponentsAdded && !bSetBackupSucceededStatus)
        {
            SetWriterComponentsBackupSucceeded(bBackupSucceeded);
        }

        SAFE_RELEASE(pAsync);
        m_pVssObject->AbortBackup();
        ReleaseVssObject();
        throw;
    }
            
    m_pVssObject->BackupComplete(&pAsync);
    WaitAndCheckForAsyncOperation(pAsync);
    try {
        VSS_ID not_deleted;
        LONG deleted;
        CHECK_COM(m_pVssObject->DeleteSnapshots(m_snapshotSetId, VSS_OBJECT_SNAPSHOT_SET, true, &deleted, &not_deleted));
    }
    catch(...) {
    }
    SAFE_RELEASE(pAsync);
    ReleaseVssObject();
}

void CVssClient::CollectWriterComponentInformation()
{
    
    // Gather writer metadata, this call can be performed only once per IVssBackupComponents instance!
    DBGPRINT(("Gathering metadata for writers on the system.\n"));
    CComPtr<IVssAsync>  pAsync;
    CHECK_COM(m_pVssObject->GatherWriterMetadata(&pAsync));

    // Wait for the gather writer metadata operation to complete. 
    WaitAndCheckForAsyncOperation(pAsync);

    DBGPRINT(("Gathered metadata for writers on the system.\n"));

    // Now traverse through all the writers and initialize the internal data structure with the 
    // details for this writer. 
    UINT cWriters = 0;
    CHECK_COM(m_pVssObject->GetWriterMetadataCount(&cWriters));
    for(UINT i = 0; i < cWriters; i++)
    {
        VSS_ID instanceId = GUID_NULL;
        CComPtr<IVssExamineWriterMetadata> ptrWriterMetadata = NULL;
        CHECK_COM(m_pVssObject->GetWriterMetadata(i, &instanceId, &ptrWriterMetadata));
        CXenVssWriter writer(ptrWriterMetadata);
        m_writerList.push_back(writer);
    }    

    // Now free the writer metadata gathered earlier, we have no use for it now. 
    CHECK_COM(m_pVssObject->FreeWriterMetadata());
}

bool CVssClient::AddWriterComponents()
{
    try
    {
        // If anything fails in this process do not add any components so that 
        // our backup works without writers, rather than not at all
    
        // first collect information about various writers and components and store 
        // into the internal data structures for the same. 
        DBGPRINT(("Collecting writer component information.\n"));
        CollectWriterComponentInformation();

        // Select components for backup
        DBGPRINT(("Now excluding writers and components based on various factors. \n"));
        SelectComponentsForBackup();
    }

    catch(...)
    {
        DBGPRINT(("There was an exception while selecting VSS components to backup. Not including VSS writers in this backup.\n"));
        m_writerList.clear();
        return false;
    }

    // Now that we have prepared a list of components to be added for this snapshot operation.
    // add them formally using the VSS framework. This will ensure that the corresponding writers 
    // quiesce the application while the snapshot is going on.
    AddSelectedComponentsForBackup();
    DBGPRINT(("Leaving add writer components\n"));
    return true;
}

void CVssClient::SetWriterComponentsBackupSucceeded(const bool bBackupSucceeded) 
{
    DBGPRINT(("Setting status of backup for explicitly included components.\n"));
    
    list<CXenVssWriter>::iterator iter = m_writerList.begin();
    for(; iter != m_writerList.end(); iter++)
    {
        DBGPRINT(("Looking at writer %S.\n", iter->GetName().c_str()));
        
        const CXenVssComponent *pComponent = NULL;

        if(pComponent = iter->GetFirstComponent())
        {
            do
            {
                if(!pComponent->GetNotifyOnBackupComplete() )
                    continue;
            
                DBGPRINT(("Setting backup status for component %S from writer %S.\n", pComponent->GetName().c_str(), iter->GetName().c_str()));
                CHECK_COM(m_pVssObject->SetBackupSucceeded(WString2Guid(iter->GetInstanceId()), 
                                                    WString2Guid(iter->GetWriterId()), 
                                                    pComponent->GetType(),
                                                    pComponent->GetLogicalPath().c_str(),
                                                    pComponent->GetName().c_str()
                                                    , bBackupSucceeded));
            }
            while(pComponent = iter->GetNextComponent());
        }
    }
}

void CVssClient::AddSelectedComponentsForBackup() 
{
    DBGPRINT(("Adding explicitly included components to the backup set.\n"));
    for(list<CXenVssWriter>::iterator iter = m_writerList.begin();
        iter != m_writerList.end();
        iter++)
    {
        DBGPRINT(("Looking at writer %S.\n", iter->GetName().c_str()));
        const CXenVssComponent *pComponent = NULL;

        if(pComponent = iter->GetFirstComponent())
        {
            do
            {         
                DBGPRINT(("Adding component %S from writer %S to the backup set.\n", pComponent->GetName().c_str(), iter->GetName().c_str()));
                CHECK_COM(m_pVssObject->AddComponent(WString2Guid(iter->GetInstanceId()), 
                                                    WString2Guid(iter->GetWriterId()), 
                                                    pComponent->GetType(),
                                                    pComponent->GetLogicalPath().c_str(),
                                                    pComponent->GetName().c_str()));
            }
            while(pComponent = iter->GetNextComponent());
        }
    }
}

void CVssClient::SelectComponentsForBackup()
{         
    DBGPRINT(("Finding which components need to be added to the backup set. \n"));
    bool bVolumeOutsideShadowCopy = false;  
    bool bComponentsDeleted = false;

    for(list<CXenVssWriter>::iterator iter = m_writerList.begin();
        iter != m_writerList.end();)
    {   
        // 1) Go through each writer
        DBGPRINT(("Looking at writer %S.\n", iter->GetName().c_str()));
        
        // 2) For this writer enumerate components
        CXenVssComponent *pComponent = NULL;
        
        if(!(pComponent = iter->GetFirstComponentInternal()))
        {
            // if no components remove writer and enumerate the writers again.
            DBGPRINT(("The writer %S has no components. Remove it from the list, and continue looking at other writers.\n", iter->GetName().c_str()));
            iter = m_writerList.erase(iter);
            continue;
        }

        // else go through each component. 
        do
        {
            DBGPRINT(("Looking at component %S.\n", pComponent->GetName().c_str()));
            if(pComponent->HasBeenExamined())
                continue;
            else
                iter->SetComponentExamined(pComponent->GetFullPath());

            // get affected volumes for this component
            int nAffectedVolumeCount = pComponent->GetAffectedVolumesCount();

            for(int nAffectedVolumes = 0; nAffectedVolumes < nAffectedVolumeCount; nAffectedVolumes++)
            {
                if(!FindObjectInList<wstring>(m_volumesList, pComponent->GetAffectedVolumeAtIndex(nAffectedVolumes)))
                {
                    DBGPRINT(("Excluding component %S, from writer %S, because it has some files outside of the volumes being snapshotted.\n", 
                        pComponent->GetName().c_str(),
                        iter->GetName().c_str()));                            
                    
                    bVolumeOutsideShadowCopy = true;                            
                    break;
                }
            }

            if(bVolumeOutsideShadowCopy)
            {
                // Delete this component and its ancestor from the list of components in the writer.
                DBGPRINT(("Deleting component %S and its ancestors from the list since the component has volumes outside of the volume shadow copy set.\n", 
                    pComponent->GetName().c_str()));
                bool bTopLevelNonSelectableComponentDeleted = false;
                iter->DeleteComponentAndAncestors(*pComponent, bTopLevelNonSelectableComponentDeleted);
                if(bTopLevelNonSelectableComponentDeleted)
                {
                    // A top-level non-selectable component was deleted, this writer should be deleted as well. 
                    DBGPRINT(("A top-level non-selectable component was deleted, hence the writer %S should be deleted as well.", iter->GetName().c_str()));
                    iter = m_writerList.erase(iter);
                }

                bComponentsDeleted = true;
                bVolumeOutsideShadowCopy = false;
                break;
            }            

            // Else check if this is a selectable component
            // if it is delete the whole component tree below this component
            DBGPRINT(("Checking to see if the component %S from the writer %S is selectable.", pComponent->GetName().c_str(), iter->GetName().c_str()));
            if(pComponent->GetIsSelectable())
            {
                DBGPRINT(("The component %S from the writer %S is selectable. Trying to delete all the descendents of this component as they should not be explicitly added.", pComponent->GetName().c_str(), iter->GetName().c_str()));                
                if(!iter->DeleteAllDescendentsForThisComponent(*pComponent))
                {
                    DBGPRINT(("No descendents found for the component %S from the writer %S.", pComponent->GetName().c_str(), iter->GetName().c_str()));
                }
                else
                {
                    bComponentsDeleted = true;
                    break;
                }

            }
        }            
        while(pComponent = iter->GetNextComponentInternal());

        if(!bComponentsDeleted)
        {
            DBGPRINT(("None of the components from the writer %S were deleted from the list. Move to the next writer.", iter->GetName().c_str()));
            iter++;            
        }
        else
        {
            DBGPRINT(("Some components from this writer were deleted from the list. Enumerate the components again."));
            bComponentsDeleted = false;
        }
    }
}
