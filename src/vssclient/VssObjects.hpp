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
 
#ifndef _VSS_OBJECTS_H
#define _VSS_OBJECTS_H

#include <windows.h>
#include <vector>
#include <map>
#include <set>
#include "vss.h"
#include "vswriter.h"
#include "vsbackup.h"

using namespace std;

// The type of a file descriptor
typedef enum 
{
    VSS_FDT_UNDEFINED = 0,
    VSS_FDT_EXCLUDE_FILES,
    VSS_FDT_FILELIST,
    VSS_FDT_DATABASE,
    VSS_FDT_DATABASE_LOG,
} VSS_DESCRIPTOR_TYPE;

// ************************ Struct representing a VSS file descriptor ************************

struct XenVssFileDescriptor
{
    XenVssFileDescriptor(IVssWMFiledesc * pFileDesc, 
                            VSS_DESCRIPTOR_TYPE type);

    wstring             m_wstrPath;
    wstring             m_wstrFilespec;
    wstring             m_wstrAlternatePath;
    bool                m_bIsRecursive;

    VSS_DESCRIPTOR_TYPE m_type;
    wstring             m_wstrExpandedPath;
    wstring             m_wstrAffectedVolume;
};

// ************************ Class representing a VSS component ************************
class CXenVssComponent{

private:
    wstring             m_wstrName;
    wstring             m_wstrWriterName;
    wstring             m_wstrLogicalPath;
    wstring             m_wstrCaption;
    VSS_COMPONENT_TYPE  m_type;
    bool                m_bIsSelectable;
    bool                m_bNotifyOnBackupComplete;
    wstring             m_wstrFullPath;
    bool                m_bIsTopLevel;
    vector<wstring>     m_affectedPaths;
    vector<wstring>     m_affectedVolumes;
    vector<XenVssFileDescriptor> m_descriptors;
    bool                m_bHasBeenExamined;
    set<wstring>        m_listChildComponentFullPaths;
    set<wstring>::iterator m_childIterator;

public:
    CXenVssComponent() {} ;
    CXenVssComponent(wstring writerNameParam, IVssWMComponent * pComponent);

    // Set function for m_bIsTopLevel
    bool GetIsTopLevel() const { return m_bIsTopLevel; }
    void SetIsTopLevel(const bool value) { m_bIsTopLevel = value; }    

    // m_IsSelectable
    bool GetIsSelectable() const { return m_bIsSelectable; }
 
    // m_wstrFullPath
    wstring GetFullPath() const { return m_wstrFullPath; }

    //m_type
    VSS_COMPONENT_TYPE GetType() const { return m_type; }

    // m_wstrLogicalPath
    wstring GetLogicalPath() const { return m_wstrLogicalPath; }

    // Get parent component full path
    wstring GetParentFullPath() const;

    // m_wstrName
    wstring GetName() const { return m_wstrName; }
    
    // m_bNotifyOnBackupComplete
    bool GetNotifyOnBackupComplete() const { return m_bNotifyOnBackupComplete; }

    // Get the number of affected volumes
    int GetAffectedVolumesCount() const 
    { 
        int nLen = static_cast<int>(m_affectedVolumes.size()); 
        return nLen;
    }

    // Get the affected volume at a specified index
    const wstring& GetAffectedVolumeAtIndex(const int index) const { return m_affectedVolumes[index]; }        

    // m_bHasBeenExamined
    bool HasBeenExamined() const { return m_bHasBeenExamined; }
    void SetHasBeenExamined() { m_bHasBeenExamined = true; }

    // Add a child component to m_listChildComponentFullPaths
    void AddChild(const wstring & fullPath);

    // Remove a child component from m_listChildComponentFullPaths
    void RemoveChild(const wstring& fullPath);

    // Get the first child from the list, this sets the context for later GetNextChildcalls
    bool GetFirstChild(wstring& firstComponentFullPath);

    // Get the next child from the list. 
    bool GetNextChild(wstring& nextComponentFullPath);            
};

// ************************ Class representing a VSS writer ******  ******************
class CXenVssWriter
{
public:
    friend class CVssClient;

private:
    wstring                     m_wstrName;
    wstring                     m_wstrId;
    wstring                     m_wstrInstanceId;
    map<wstring, CXenVssComponent>        m_mapComponents;
    map<wstring, CXenVssComponent>::iterator        m_compIterator;
    vector<XenVssFileDescriptor>   m_excludedFiles;
    VSS_WRITERRESTORE_ENUM      m_writerRestoreConditions;
    bool                        m_bSupportsRestore;
    VSS_RESTOREMETHOD_ENUM      m_restoreMethod;
    bool                        m_bRebootRequiredAfterRestore;
      
    // Get the first component from the list, this sets the context for later GetNextComponent calls
    CXenVssComponent* GetFirstComponentInternal();

    // Get the next component from the list. 
    CXenVssComponent* GetNextComponentInternal();
    
public: 
    
    CXenVssWriter(IVssExamineWriterMetadata * pMetadata);
    
    // Get and set functions
    // m_wstrId
    wstring GetWriterId() const { return m_wstrId; }
    
    // m_wstrInstanceId;
    wstring GetInstanceId() const { return m_wstrInstanceId; }

    // m_wstrName
    wstring GetName() const { return m_wstrName; }

    // SetCurrentComponentExamined
    void SetComponentExamined(const wstring& path) { m_mapComponents[path].SetHasBeenExamined(); }

    // Get the first component from the list, this sets the context for later GetNextComponent calls
    const CXenVssComponent* GetFirstComponent();

    // Get the next component from the list. 
    const CXenVssComponent* GetNextComponent();
 
    // Delete the passed component and its ancestors from the list. 
    void DeleteComponentAndAncestors(const CXenVssComponent& component, bool &bTopLevelNonSelectableComponentDeleted);

    // Delete all the descendents of the passed component from the list. 
    // Return true if at least descendent was found and deleted. 
    bool DeleteAllDescendentsForThisComponent(CXenVssComponent& component);

    // Set the parent and child dependencies for the passed in component.
    void SetComponentDependencies(CXenVssComponent& component);
};

// ************************ WStringBuffer declaration ************************
class WString2Buffer
{
private: 
    wstring &       m_s;
    vector<WCHAR>   m_sv;

public:

    WString2Buffer(wstring & s);
    ~WString2Buffer();

    // Return the temporary WCHAR buffer 
    operator WCHAR* ();

    // Return the available size of the temporary WCHAR buffer 
    size_t length();
};

// ************************ FindObjectInList ************************
// This template function searches for an object in a vector and
// returns true or false accordingly. 
// ******************************************************************
template <class T>
bool FindObjectInList(vector<T> list, const T& object)
{
    for(vector<T>::iterator iter = list.begin(); iter != list.end(); iter++)
    {
        if(*iter == object)
            return true;
    }

    return false;
}


#endif // _VSS_OBJECTS_H