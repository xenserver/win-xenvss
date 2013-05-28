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
#include <stdio.h>
#include <stdlib.h>

#include <winioctl.h>
#include <ntddstor.h>

enum SNAPSHOT_TYPE{
    SNAPSHOT_TYPE_VM, 
    SNAPSHOT_TYPE_VOLUME
};

typedef bool (__stdcall *SAVEBACKUPDOC_CALLBACK)(char *);

typedef void* (__stdcall *PVssClientInit)(SNAPSHOT_TYPE);
typedef void (__stdcall *PVssClientAddVolume)(void*, wchar_t*);
typedef bool (__stdcall *PVssClientCreateSnapshotSet)(void*, SAVEBACKUPDOC_CALLBACK);
typedef void (__stdcall *PVssClientDestroy)(void*);


void DebugPrint(char* Fmt, ...)
{
    static char buffer[4096];
    va_list args;

    va_start(args, Fmt);
    wvsprintf(buffer, Fmt, args);
    va_end(args);

    OutputDebugStringA(buffer);
    printf(buffer);
}

bool __stdcall callback(char* ptr)
{
    DebugPrint("\n\nin callback 0x%p (%ws)\n", ptr, (wchar_t*)ptr);
    return true;
}

// Entry point
//extern "C" __cdecl void _WinMain(int argc, char ** argv)
//extern "C" int __cdecl main(int argc, char** argv)
//int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR cmd, int show)
extern "C" int __cdecl main(int argc, char** argv)
{
    PVssClientInit vssinit;
    PVssClientAddVolume vssaddv;
    PVssClientCreateSnapshotSet vsscsss;
    PVssClientDestroy vssterm;
    void* handle;

    HMODULE mod = LoadLibrary("vssclient.dll");
    if (mod) {
        DebugPrint("\n\nLoaded Module\n");
        vssinit = (PVssClientInit)GetProcAddress(mod, "VssClientInit");
        vssaddv = (PVssClientAddVolume)GetProcAddress(mod, "VssClientAddVolume");
        vsscsss = (PVssClientCreateSnapshotSet)GetProcAddress(mod, "VssClientCreateSnapshotSet");
        vssterm = (PVssClientDestroy)GetProcAddress(mod, "VssClientDestroy");

        DebugPrint("\n\nFunctions 0x%p 0x%p 0x%p 0x%p\n", vssinit, vssaddv, vsscsss, vssterm);

        if (vssinit && vssaddv && vsscsss && vssterm) {
            handle = vssinit(SNAPSHOT_TYPE_VM);
            DebugPrint("\n\nHandle 0x%p\n", handle);
            if (handle) {
                void* vols;
                wchar_t volume[MAX_PATH];

                vols = FindFirstVolumeW(volume, sizeof(volume)/sizeof(wchar_t));
                DebugPrint("\n\nBegin adding volumes 0x%p\n", vols);
                if (vols) {
                    do {
                        DebugPrint("\n\nAdding %ws\n", volume);
                        vssaddv(handle, volume);
                    } while (FindNextVolumeW(vols, volume, sizeof(volume)/sizeof(wchar_t)));
                    FindVolumeClose(vols);
                }
                DebugPrint("\n\nEnd adding volumes\n");

                if (vsscsss(handle, callback)) {
                    DebugPrint("\n\nsucceeded\n");
                } else {
                    DebugPrint("\n\nfailed\n");
                }

                vssterm(handle);
                DebugPrint("\n\nterminated\n");
            }
        }

        DebugPrint("\n\nfree library\n");
        FreeLibrary(mod);
    }
    DebugPrint("\n\nfinished\n");
    return 0;
}

