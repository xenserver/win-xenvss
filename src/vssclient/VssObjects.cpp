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

#include <iostream> 
#include <algorithm>
#include "atlbase.h"
#include "strsafe.h"

using namespace std; 

#include "VssObjects.hpp"
#include "CVssClient.hpp"

// *************************** Miscellaneous macros *************************** //
#define CHECK_COM_RETURN(x)    ThrowIfComError(x)

// Helper macros to print a GUID using printf-style formatting
#define WSTR_GUID_FMT  L"{%.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x}"

#define GUID_PRINTF_ARG( X )                                \
    (X).Data1,                                              \
    (X).Data2,                                              \
    (X).Data3,                                              \
    (X).Data4[0], (X).Data4[1], (X).Data4[2], (X).Data4[3], \
    (X).Data4[4], (X).Data4[5], (X).Data4[6], (X).Data4[7]

// *************************** End of miscellaneous macros *************************** //

// *************************** Miscellaneous utility functions *************************** //
// Most should be inline unless they are too long or are recursive

/******************************************************************************
 *                              ThrowIfComError()
 ******************************************************************************/
inline void ThrowIfComError(HRESULT code)
{
    if (FAILED(code)) 
    { 
        throw code;
    }
}

// Converts a GUID to a wstring
inline wstring Guid2WString(GUID guid)
{
    wstring guidString(100, L'\0');
    CHECK_COM_RETURN(StringCchPrintfW(WString2Buffer(guidString), guidString.length(), WSTR_GUID_FMT, GUID_PRINTF_ARG(guid)));

    return guidString;
}

// Convert the given BSTR (potentially NULL) into a valid wstring
inline wstring BSTR2WString(BSTR bstr)
{
    return (bstr == NULL)? wstring(L""): wstring(bstr);
}

// Append a backslash to the current string 
inline wstring AppendBackslash(wstring str)
{
    if (str.length() == 0)
        return wstring(L"\\");
    if (str[str.length() - 1] == L'\\')
        return str;
    return str.append(L"\\");
}

// Get the unique volume name for the given path
inline wstring GetUniqueVolumeNameForPath(wstring path)
{    
    _ASSERTE(path.length() > 0);

    // Add the backslash termination, if needed
    path = AppendBackslash(path);

    // Get the root path of the volume
    wstring volumeRootPath(MAX_PATH, L'\0');
    if(GetVolumePathNameW((LPCWSTR)path.c_str(), WString2Buffer(volumeRootPath), (DWORD)volumeRootPath.length()) == 0)
        throw "GetVolumePathName failed.";
    
    // Get the volume name alias (might be different from the unique volume name in rare cases)
    wstring volumeName(MAX_PATH, L'\0');
    if(GetVolumeNameForVolumeMountPointW((LPCWSTR)volumeRootPath.c_str(), WString2Buffer(volumeName), (DWORD)volumeName.length()) == 0)
        throw "GetVolumeNameForVolumeMountPoint failed."; 
    
    // Get the unique volume name
    wstring volumeUniqueName(MAX_PATH, L'\0');
    if(GetVolumeNameForVolumeMountPointW((LPCWSTR)volumeName.c_str(), WString2Buffer(volumeUniqueName), (DWORD)volumeUniqueName.length()) == 0)
        throw "GetVolumeNameForVolumeMountPoint failed." ;
    
    return volumeUniqueName;
}

// *************************** End of miscellaneous utility functions *************************** //

// *************************** WString2Buffer member function definitions *************************** // 
// constructor
WString2Buffer::WString2Buffer(wstring & s): 
        m_s(s), m_sv(s.length() + 1, L'\0')
{
    // Move data from wstring to the temporary vector
    std::copy(m_s.begin(), m_s.end(), m_sv.begin());
}

// destructor
WString2Buffer::~WString2Buffer()
{
    // Move data from the temporary vector to the string
    m_sv.resize(wcslen(&m_sv[0]));
    m_s.assign(m_sv.begin(), m_sv.end());
}

// Return the temporary WCHAR buffer 
WString2Buffer::operator WCHAR* () 
{ 
    return &(m_sv[0]); 
}

// Return the available size of the temporary WCHAR buffer 
size_t WString2Buffer::length() 
{ 
    return m_s.length(); 
}

// *************************** End of WString2Buffer member function definitions *************************** // 

// *************************** XenVssFileDescriptor member function definitions *************************** //  
// Construct the object using a IVssWMFiledesc object and a file descriptor type enum value.
XenVssFileDescriptor::XenVssFileDescriptor(
        IVssWMFiledesc * pFileDesc, 
        VSS_DESCRIPTOR_TYPE type
        )
{
    // Set the type
    m_type= type;

    CComBSTR bstrPath;
    CHECK_COM_RETURN(pFileDesc->GetPath(&bstrPath));

    CComBSTR bstrFilespec;
    CHECK_COM_RETURN(pFileDesc->GetFilespec (&bstrFilespec));

    bool bRecursive = false;
    CHECK_COM_RETURN(pFileDesc->GetRecursive(&bRecursive));

    CComBSTR bstrAlternate;
    CHECK_COM_RETURN(pFileDesc->GetAlternateLocation(&bstrAlternate));

    // Initialize local data members
    m_wstrPath = BSTR2WString(bstrPath);
    m_wstrFilespec = BSTR2WString(bstrFilespec);
    m_wstrAlternatePath = BSTR2WString(bstrAlternate);
    m_bIsRecursive = bRecursive;
    
    // Get the expanded path
    m_wstrExpandedPath.resize(MAX_PATH, L'\0');
    _ASSERTE(bstrPath && bstrPath[0]);
    DWORD dwRet = ExpandEnvironmentStringsW(bstrPath, (PWCHAR)m_wstrExpandedPath.c_str(), (DWORD)m_wstrExpandedPath.length());
    if(dwRet == 0) 
    {
        throw;
    }

    m_wstrExpandedPath = AppendBackslash(m_wstrExpandedPath);

    // Get the affected volume 
    m_wstrAffectedVolume = GetUniqueVolumeNameForPath(m_wstrExpandedPath);
}

// *************************** End of XenVssFileDescriptor member function definitions *************************** //  

// *************************** CXenVssComponent member function definitions *************************** //  
// Construct the object using the name of the writer and a IVssWMComponent object
CXenVssComponent::CXenVssComponent(wstring writerNameParam, IVssWMComponent * pComponent) 
    : m_bHasBeenExamined(false)
{
    m_wstrWriterName = writerNameParam;

    // Get the component info
    PVSSCOMPONENTINFO pInfo = NULL;
    CHECK_COM_RETURN(pComponent->GetComponentInfo (&pInfo));

    // Initialize local members
    m_wstrName = BSTR2WString(pInfo->bstrComponentName);
    m_wstrLogicalPath = BSTR2WString(pInfo->bstrLogicalPath);
    m_wstrCaption = BSTR2WString(pInfo->bstrCaption);
    m_type = pInfo->type;
    m_bIsSelectable = pInfo->bSelectable;
    m_bNotifyOnBackupComplete = pInfo->bNotifyOnBackupComplete;

    // Compute the full path    
    m_wstrFullPath = AppendBackslash(m_wstrLogicalPath) + m_wstrName;
    if (m_wstrFullPath[0] != L'\\')
        m_wstrFullPath = wstring(L"\\") + m_wstrFullPath;

    // Get file list descriptors
    for(unsigned i = 0; i < pInfo->cFileCount; i++)
    {
        CComPtr<IVssWMFiledesc> pFileDesc;
        CHECK_COM_RETURN(pComponent->GetFile (i, &pFileDesc));

        XenVssFileDescriptor desc(pFileDesc, VSS_FDT_FILELIST);
        m_descriptors.push_back(desc);
    }
    
    // Get database descriptors
    for(unsigned i = 0; i < pInfo->cDatabases; i++)
    {
        CComPtr<IVssWMFiledesc> pFileDesc;
        CHECK_COM_RETURN(pComponent->GetDatabaseFile (i, &pFileDesc));

        XenVssFileDescriptor desc(pFileDesc, VSS_FDT_DATABASE);
        m_descriptors.push_back(desc);
    }
    
    // Get log descriptors
    for(unsigned i = 0; i < pInfo->cLogFiles; i++)
    {
        CComPtr<IVssWMFiledesc> pFileDesc;
        CHECK_COM_RETURN(pComponent->GetDatabaseLogFile (i, &pFileDesc));

        XenVssFileDescriptor desc(pFileDesc, VSS_FDT_DATABASE_LOG);
        m_descriptors.push_back(desc);
    }
    
    pComponent->FreeComponentInfo (pInfo);

    // Compute the affected paths and volumes
    for(unsigned i = 0; i < m_descriptors.size(); i++)
    {
        if (!FindObjectInList<wstring>(m_affectedPaths, m_descriptors[i].m_wstrExpandedPath))
            m_affectedPaths.push_back(m_descriptors[i].m_wstrExpandedPath);

        if (!FindObjectInList<wstring>(m_affectedVolumes, m_descriptors[i].m_wstrAffectedVolume))
            m_affectedVolumes.push_back(m_descriptors[i].m_wstrAffectedVolume);
    }

    std::sort(m_affectedPaths.begin(), m_affectedPaths.end());
}

// Add a child component to m_listChildComponentFullPaths
void CXenVssComponent::AddChild(const wstring & fullPath)
{
    m_listChildComponentFullPaths.insert(fullPath);
}

// Remove a child component pointer to m_mapChildComponents
void CXenVssComponent::RemoveChild(const wstring& fullPath)
{
    m_listChildComponentFullPaths.erase(fullPath);
}

// Get the first child from the list, this sets the context for later GetNextChildcalls
bool CXenVssComponent::GetFirstChild(wstring& firstComponentFullPath)
{
    if(m_listChildComponentFullPaths.begin() == m_listChildComponentFullPaths.end())
        return false;

    m_childIterator = m_listChildComponentFullPaths.begin();
    firstComponentFullPath = *m_childIterator;
    m_childIterator++;
    return true;
}

// Get the next child from the list. 
bool CXenVssComponent::GetNextChild(wstring& nextComponentFullPath)
{
    if(m_childIterator == m_listChildComponentFullPaths.end())
        return false;

    nextComponentFullPath = *m_childIterator;
    m_childIterator++;
    return true;
}

wstring CXenVssComponent::GetParentFullPath() const
{
    wstring wstrFullPath(m_wstrLogicalPath);
    if (wstrFullPath[0] != L'\\')
        wstrFullPath = wstring(L"\\") + wstrFullPath;

    return wstrFullPath;
}

// *************************** End of CXenVssComponent member function definitions *************************** //  

// *************************** CXenVssWriter member function definitions *************************** //  
// Construct the object using a IVssExamineWriterMetadata object    
CXenVssWriter::CXenVssWriter(IVssExamineWriterMetadata * pMetadata): m_compIterator(m_mapComponents.begin())
{
    // Get the writer identity 
    VSS_ID instanceId  = GUID_NULL, writerId = GUID_NULL;
    BSTR bstrWriterName; 
    VSS_USAGE_TYPE usageType = (VSS_USAGE_TYPE)0;
    VSS_SOURCE_TYPE sourceType = (VSS_SOURCE_TYPE)0;

    CHECK_COM_RETURN(pMetadata->GetIdentity(&instanceId, &writerId, &bstrWriterName, &usageType, &sourceType));

    // Get the restore method 
    VSS_RESTOREMETHOD_ENUM restoreMethod; 
    BSTR bstrService, bstrUserProcedure;
    VSS_WRITERRESTORE_ENUM writerRestore;
    UINT nMappings = 0;
    CHECK_COM_RETURN(pMetadata->GetRestoreMethod(
        &m_restoreMethod,
        &bstrService,
        &bstrUserProcedure,
        &m_writerRestoreConditions,
        &m_bRebootRequiredAfterRestore,
        &nMappings
        ));

    // Initialize local members
    m_wstrName = BSTR2WString(bstrWriterName);
    m_wstrId = Guid2WString(writerId);
    m_wstrInstanceId = Guid2WString(instanceId);
    m_bSupportsRestore = (m_writerRestoreConditions != VSS_WRE_NEVER);

    // Get file counts      
    unsigned cIncludeFiles = 0;
    unsigned cExcludeFiles = 0;
    unsigned cComponents = 0;
    CHECK_COM_RETURN(pMetadata->GetFileCounts(&cIncludeFiles, &cExcludeFiles, &cComponents));
 
    // Get exclude files
    for(unsigned i = 0; i < cExcludeFiles; i++)
    {
        CComPtr<IVssWMFiledesc> pFileDesc;
        CHECK_COM_RETURN(pMetadata->GetExcludeFile(i, &pFileDesc));

        // Add this descriptor to the list of excluded files
        XenVssFileDescriptor excludedFile(pFileDesc, VSS_FDT_EXCLUDE_FILES);
        m_excludedFiles.push_back(excludedFile);
    }

    // Enumerate components
    for(unsigned iComponent = 0; iComponent < cComponents; iComponent++)
    {
        // Get component
        CComPtr<IVssWMComponent> pComponent;
        CHECK_COM_RETURN(pMetadata->GetComponent(iComponent, &pComponent));

        // Add this component to the list of components
        CXenVssComponent component(m_wstrName, pComponent);        
        m_mapComponents.insert(std::pair<wstring, CXenVssComponent>(component.GetFullPath(),component));
    }        

    // Now set component dependencies
    for(map<wstring, CXenVssComponent>::iterator iter = m_mapComponents.begin();
            iter != m_mapComponents.end();
            iter++)
    {
        SetComponentDependencies(iter->second);
    }
}

void CXenVssWriter::DeleteComponentAndAncestors(const CXenVssComponent& component, bool &bTopLevelNonSelectableComponentDeleted)
{ 
    CXenVssComponent *pComponent = const_cast<CXenVssComponent *> (&component);

    while(true)
    {
        wstring wstrParentPath = pComponent->GetParentFullPath();
        bool bIsTopLevel = pComponent->GetIsTopLevel(), bIsSelectable = pComponent->GetIsSelectable();
        m_mapComponents.erase(pComponent->GetFullPath());
        if(bIsTopLevel)
        {
            if(!bIsSelectable)
            {
                // This was a non-selectable top-level component, flag this fact.
                bTopLevelNonSelectableComponentDeleted = true;            
            }

            break;
        }

        pComponent = &m_mapComponents[wstrParentPath];
    }       
} 

bool CXenVssWriter::DeleteAllDescendentsForThisComponent(CXenVssComponent& component)
{
    bool bRetVal = false;
    wstring childFullPath;
    
    if(!component.GetFirstChild(childFullPath))
    {
        return bRetVal;
    }

    // We have at least one child now so start deleting children
    do
    {
        component.RemoveChild(childFullPath);
        m_mapComponents.erase(m_mapComponents.find(childFullPath));        
        bRetVal = true;
    }
    while(component.GetNextChild(childFullPath));
   
    return bRetVal;
}

CXenVssComponent* CXenVssWriter::GetFirstComponentInternal() 
{
    if(m_mapComponents.begin() == m_mapComponents.end())
        return NULL;

    m_compIterator = m_mapComponents.begin();
    CXenVssComponent* pComponent = &(m_compIterator->second);
    m_compIterator++;
    return pComponent;
}

const CXenVssComponent* CXenVssWriter::GetFirstComponent()
{
    return GetFirstComponentInternal();
}

CXenVssComponent* CXenVssWriter::GetNextComponentInternal()
{
    if(m_compIterator == m_mapComponents.end())
        return NULL;

    CXenVssComponent* pComponent = &(m_compIterator->second);
    m_compIterator++;
    return pComponent;
}

const CXenVssComponent* CXenVssWriter::GetNextComponent()
{
    return GetNextComponentInternal();
}

void CXenVssWriter::SetComponentDependencies(CXenVssComponent& component)
{
    // First find if this component is a top-level component
    if(m_mapComponents.find(component.GetParentFullPath()) == m_mapComponents.end())
    { 
        component.SetIsTopLevel(true);
    }
    else
    {
        // Now populate the child map for this component's parent component with this component
        m_mapComponents[component.GetParentFullPath()].AddChild(component.GetFullPath());
    }
}

// *************************** End of CXenVssWriter member function definitions *************************** //  
