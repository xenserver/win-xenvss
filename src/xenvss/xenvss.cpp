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

#include <includes.h>
#include "xenvss_i.h" // generated

#include "resource.h"
#include "version.h"
#include "debug.h"

class CXenVssModule : public ATL::CAtlDllModuleT< CXenVssModule >
{
public :
	DECLARE_LIBID(LIBID_XenVssLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_XENVSS, "{22F6642E-0005-4941-9DCE-C9DB45486387}")
};
class CXenVssModule _AtlModule;

//-VSS IDs---------------------------------------
static wchar_t* WSZ_XenVssProviderName    = L"Citrix XEN VSS Provider";
static wchar_t* WSZ_XenVssProviderVer     = L"1.0";

static const GUID GUID_XenVssProviderId   = 
{0x3aeb8223, 0xa8eb, 0x43a2, { 0x8f, 0xf7, 0x86, 0x83, 0x12, 0xe6, 0x7a, 0x8f }}; // {3AEB8223-A8EB-43a2-8FF7-868312E67A8F}
static const GUID GUID_XenVssProviderVer  = 
{0xb23902ff, 0x6fba, 0x4a9f, { 0x93, 0xd4, 0x31, 0x4e, 0x78, 0x64, 0x05, 0x66 }}; // {B23902FF-6FBA-4a9f-93D4-314E78640566}
//-----------------------------------------------

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	return _AtlModule.DllMain(dwReason, lpReserved); 
}
STDAPI DllCanUnloadNow(void)
{
    return _AtlModule.DllCanUnloadNow();
}
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID* ppv)
{
    return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}
STDAPI DllRegisterServer(void)
{
	HRESULT     hr;
    IVssAdmin*  VssAdmin;

    Trace("====>\n");

    hr = _AtlModule.DllRegisterServer();
    TraceIfFailed(hr, "_AtlModule.DllRegisterServer()");
    if (FAILED(hr))
        goto out;

    hr = CoCreateInstance(CLSID_VSSCoordinator, NULL, CLSCTX_ALL, 
                        IID_IVssAdmin, (void**)&VssAdmin);
    TraceIfFailed(hr, "CoCreateInstance(...)");
    if (FAILED(hr))
        goto out;

    hr = VssAdmin->RegisterProvider(GUID_XenVssProviderId, CLSID_XenVssProvider,
                            WSZ_XenVssProviderName, VSS_PROV_HARDWARE, 
                            WSZ_XenVssProviderVer, GUID_XenVssProviderVer);
    TraceIfFailed(hr, "VssAdmin->RegisterProvider(...)");
    VssAdmin->Release();
    if (FAILED(hr))
        goto out;

    // RegSetValue
    // HKLM\System\CurrentControlSet\Services\EventLog\Application\<PROG_ID>\CustomSource = 1
    // HKLM\System\CurrentControlSet\Services\EventLog\Application\<PROG_ID>\TypesSupported = 7
    // HKLM\System\CurrentControlSet\Services\EventLog\Application\<PROG_ID>\EventMessageFile = <MODULE_PATH>/<MODULE_NAME>

out:
    TraceHR(hr);
    return hr;
}
STDAPI DllUnregisterServer(void)
{
    HRESULT     hr;
    IVssAdmin*  VssAdmin;
    
    Trace("====>\n");

    // RegDeleteTree
    // HKLM\System\CurrentControlSet\Services\EventLog\Application\<PROG_ID>
    
    hr = CoCreateInstance(CLSID_VSSCoordinator, NULL, CLSCTX_ALL, IID_IVssAdmin, (void**)&VssAdmin);
    TraceIfFailed(hr, "CoCreateInstance(...)");
    if (SUCCEEDED(hr)) {
        hr = VssAdmin->UnregisterProvider(GUID_XenVssProviderId);
        TraceIfFailed(hr, "VssAdmin->UnregisterProvider(...)");
        VssAdmin->Release();
    }

	hr = _AtlModule.DllUnregisterServer();
    TraceIfFailed(hr, "_AtlModule.DllUnregisterServer()");
    
    TraceHR(hr);
    return hr;
}
STDAPI DllInstall(BOOL bInstall, _In_opt_  LPCWSTR pszCmdLine)
{
	HRESULT hr = E_FAIL;
	static const wchar_t szUserSwitch[] = L"user";

    Trace("====> (%s, %ws)\n", bInstall ? "TRUE" : "FALSE", pszCmdLine);

	if (pszCmdLine != NULL)
	{
		if (_wcsnicmp(pszCmdLine, szUserSwitch, _countof(szUserSwitch)) == 0)
		{
			ATL::AtlSetPerUserRegistration(true);
		}
	}

	if (bInstall)
	{	
		hr = DllRegisterServer();
		if (FAILED(hr))
		{
			DllUnregisterServer();
		}
	}
	else
	{
		hr = DllUnregisterServer();
	}

    TraceHR(hr);
	return hr;
}


