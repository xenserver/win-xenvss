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

#define VSSAPI_EXPORTS 1
#include "vssinterface.hpp"
#include "cvssclient.hpp"
#include <string>
#include "debug.h"
static const GUID GUID_PROV_XEN = {0x3aeb8223, 0xa8eb, 0x43a2, { 0x8f, 0xf7, 0x86, 0x83, 0x12, 0xe6, 0x7a, 0x8f }}; // {3AEB8223-A8EB-43a2-8FF7-868312E67A8F}

extern "C" {

VSS_API void *VssClientInit(SNAPSHOT_TYPE type) {
    DebugPrint("New client");
    try {
        CVssClient *client = new CVssClient();
        try {
    
            DebugPrint("Set type");
            client->SetSnapshotType(type);
            DebugPrint("Init");
            client->Initialize();
            DebugPrint("Find provider");
            client->FindXenProvider();
        }
        catch(...) {
            DebugPrint("%s %x",client->GetErrorMessage().c_str(), client->GetErrorCode());
            DebugPrint("Caught error");
            return NULL;
        }
        DebugPrint("Return");
        return (void *)client;
    }
    catch(...) {
        DebugPrint("Caught error in vss client init");
        return NULL;
    }
}



VSS_API void VssGetErrorMessage(void * client, CHARALLOC_CALLBACK callback) {
    try {
        std::string str = ((CVssClient *)client)->GetErrorMessage();
        char *buffer = callback(str.size()+1);
        if (buffer) {
            strcpy_s(buffer, str.size()+1, str.c_str());
        }
    }
    catch (...) {
        DebugPrint("Error getting error message");
    }
}

VSS_API HRESULT VssGetErrorCode(void *client) {
    try {
        return  ((CVssClient *)client)->GetErrorCode();
    }
    catch (...){
        DebugPrint("Error getting error code");
        return -1;
    }
}

VSS_API HRESULT VssGetErrorState(void *client) {
    try {
        return  ((CVssClient *)client)->GetErrorState();
    }
    catch (...){
        DebugPrint("Error getting error state");
        return -1;
    }
}

VSS_API void VssClientAddVolume(void * client, WCHAR *volumeName) {
    BOOL        supported;
    HRESULT     hr;
    try {
        ((CVssClient *)client)->InitVssObject(); 
        hr = ((CVssClient *)client)->m_pVssObject->IsVolumeSupported(GUID_PROV_XEN, volumeName, &supported);
        if (SUCCEEDED(hr) && supported)
        {
            DebugPrint("Adding volume %s",volumeName);
            ((CVssClient *)client)->m_volumesList.push_back(volumeName);
        }
        ((CVssClient *)client)->ReleaseVssObject();
    }
    catch (...) {
        DebugPrint("Error adding volume");
    }
}

VSS_API bool VssClientCreateSnapshotSet(void *client, SAVEBACKUPDOC_CALLBACK callback){
    try {
        ((CVssClient *)client)->CreateSnapshotSet(callback);
    }
    catch (...) {
        return false;
    }
    return true;
}

VSS_API void VssClientDestroy(void *client) {
    delete (CVssClient *)client;
}

}
