// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

#include "precomp.h"
#include <WixToolset.Mba.Host.h> // includes the generated assembly name macros.

typedef IBootstrapperApplicationFactory* (STDMETHODCALLTYPE *PFNCREATEBAFACTORY)(
    __in LPCWSTR wzBaFactoryAssemblyName
    );

static HINSTANCE vhInstance = NULL;
static ICLRRuntimeHost4 *vpCLRHost = NULL;
static DWORD vdwDomainId = 0;


// internal function declarations

static HRESULT GetAppBase(
    __out LPWSTR* psczAppBase
);
static HRESULT GetAppDomain(
    __in LPCWSTR wzAppBase,
    __out DWORD* pdwDomainId
    );
static HRESULT GetCLRHost(
    __in LPCWSTR wzCoreClrPath,
    __out ICLRRuntimeHost4** ppCLRHost
    );
static HRESULT GetBaFactoryAssemblyName(
    __in LPCWSTR wzAppBase,
    __out LPWSTR* psczBaFactoryAssemblyName
    );
static HRESULT CreateManagedBootstrapperApplication(
    __in LPCWSTR wzBaFactoryAssemblyName,
    __in DWORD dwDomainId,
    __in const BOOTSTRAPPER_CREATE_ARGS* pArgs,
    __inout BOOTSTRAPPER_CREATE_RESULTS* pResults
    );
static HRESULT CreateManagedBootstrapperApplicationFactory(
    __in LPCWSTR wzBaFactoryAssemblyName,
    __in DWORD dwDomainId,
    __out IBootstrapperApplicationFactory** ppAppFactory
    );


// function definitions

extern "C" BOOL WINAPI DllMain(
    IN HINSTANCE hInstance,
    IN DWORD dwReason,
    IN LPVOID /* pvReserved */
    )
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        ::DisableThreadLibraryCalls(hInstance);
        vhInstance = hInstance;
        break;

    case DLL_PROCESS_DETACH:
        vhInstance = NULL;
        break;
    }

    return TRUE;
}

extern "C" HRESULT WINAPI BootstrapperApplicationCreate(
    __in const BOOTSTRAPPER_CREATE_ARGS* pArgs,
    __inout BOOTSTRAPPER_CREATE_RESULTS* pResults
    )
{
    HRESULT hr = S_OK; 
    LPWSTR sczAppBase = NULL;
    LPWSTR sczBaFactoryAssemblyName = NULL;

    hr = BalInitializeFromCreateArgs(pArgs, NULL);
    ExitOnFailure(hr, "Failed to initialize Bal.");

    hr = GetAppBase(&sczAppBase);
    BalExitOnFailure(hr, "Failed to get the host base path.");

    hr = GetBaFactoryAssemblyName(sczAppBase, &sczBaFactoryAssemblyName);
    BalExitOnFailure(hr, "Failed to get the bootstrapper assembly name.");

    hr = GetAppDomain(sczAppBase, &vdwDomainId);
    BalExitOnFailure(hr, "Failed to load .NET Core.");

    BalLog(BOOTSTRAPPER_LOG_LEVEL_STANDARD, "Loading managed bootstrapper application.");

    hr = CreateManagedBootstrapperApplication(sczBaFactoryAssemblyName, vdwDomainId, pArgs, pResults);
    BalExitOnFailure(hr, "Failed to create the managed bootstrapper application.");

LExit:
    ReleaseStr(sczBaFactoryAssemblyName);
    ReleaseStr(sczAppBase);

    return hr;
}

extern "C" void WINAPI BootstrapperApplicationDestroy()
{
    if (vdwDomainId)
    {
        HRESULT hr = vpCLRHost->UnloadAppDomain(vdwDomainId, TRUE);
        if (FAILED(hr))
        {
            BalLogError(hr, "Failed to unload app domain.");
        }
    }

    if (vpCLRHost)
    {
        vpCLRHost->Stop();
        vpCLRHost->Release();
    }

    BalUninitialize();
}

static HRESULT GetAppBase(
    __out LPWSTR* psczAppBase
)
{
    HRESULT hr = S_OK;
    LPWSTR sczFullPath = NULL;

    hr = PathForCurrentProcess(&sczFullPath, vhInstance);
    ExitOnFailure(hr, "Failed to get the full host path.");

    hr = PathGetDirectory(sczFullPath, psczAppBase);
    ExitOnFailure(hr, "Failed to get the directory of the full process path.");

LExit:
    ReleaseStr(sczFullPath);

    return hr;
}

// Gets the custom AppDomain for loading managed BA.
static HRESULT GetAppDomain(
    __in LPCWSTR wzAppBase,
    __out DWORD *pDomainId
    )
{
    HRESULT hr = S_OK;
    ICLRRuntimeHost4* pCLRHost = NULL;
    LPWSTR sczCoreClrPath = NULL;
    DWORD dwAppDomainFlags = 0;
    LPCWSTR propertyKeys[] = {
        L"APP_PATHS"
    };
    LPCWSTR propertyValues[] = {
        wzAppBase
    };

    hr = PathConcat(wzAppBase, L"coreclr.dll", &sczCoreClrPath);
    ExitOnFailure(hr, "Failed to get the full path to coreclr.");

    // Load the CLR.
    hr = GetCLRHost(sczCoreClrPath, &pCLRHost);
    ExitOnFailure(hr, "Failed to create the CLR host.");

    hr = pCLRHost->Start();
    ExitOnRootFailure(hr, "Failed to start the CLR host.");


    dwAppDomainFlags =
        // APPDOMAIN_FORCE_TRIVIAL_WAIT_OPERATIONS |        // Do not pump messages during wait
        // APPDOMAIN_SECURITY_SANDBOXED |                   // Causes assemblies not from the TPA list to be loaded as partially trusted
        APPDOMAIN_ENABLE_PLATFORM_SPECIFIC_APPS |           // Enable platform-specific assemblies to run
        APPDOMAIN_ENABLE_PINVOKE_AND_CLASSIC_COMINTEROP |   // Allow PInvoking from non-TPA assemblies
        APPDOMAIN_DISABLE_TRANSPARENCY_ENFORCEMENT;         // Entirely disables transparency checks
    pCLRHost->CreateAppDomainWithManager(L"MBA", dwAppDomainFlags, NULL, NULL, sizeof(propertyKeys) / sizeof(LPCWSTR), propertyKeys, propertyValues, pDomainId);

LExit:
    ReleaseStr(sczCoreClrPath);
    ReleaseNullObject(pCLRHost);

    return hr;
}

// Gets the CLR host and caches it.
static HRESULT GetCLRHost(
    __in LPCWSTR wzCoreClrPath,
    __out ICLRRuntimeHost4** ppCLRHost
    )
{
    HRESULT hr = S_OK;
    UINT uiMode = 0;
    HMODULE hModule = NULL;
    FnGetCLRRuntimeHost pfnGetCLRRuntimeHost = NULL;

    // Always set the error mode because we will always restore it below.
    uiMode = ::SetErrorMode(0);

    // Cache the CLR host to be shutdown later. This can occur on a different thread.
    if (!vpCLRHost)
    {
        // Disable message boxes from being displayed on error and blocking execution.
        ::SetErrorMode(uiMode | SEM_FAILCRITICALERRORS);

        hModule = ::LoadLibraryExW(wzCoreClrPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        ExitOnNullWithLastError(hModule, hr, "Failed to load coreclr.dll.");

        pfnGetCLRRuntimeHost = reinterpret_cast<FnGetCLRRuntimeHost>(::GetProcAddress(hModule, "GetCLRRuntimeHost"));
        ExitOnNullWithLastError(pfnGetCLRRuntimeHost, hr, "Failed to get procedure address for GetCLRRuntimeHost.");

        hr = pfnGetCLRRuntimeHost(IID_ICLRRuntimeHost4, reinterpret_cast<IUnknown**>(&vpCLRHost));
        ExitOnFailure(hr, "Failed to get runtime host from GetCLRRuntimeHost.");
    }

    vpCLRHost->AddRef();
    *ppCLRHost = vpCLRHost;

LExit:

    // Unload the module so it's not in use.
    if (FAILED(hr))
    {
        ::FreeLibrary(hModule);
    }

    ::SetErrorMode(uiMode); // restore the previous error mode.

    return hr;
}

static HRESULT GetBaFactoryAssemblyName(
    __in LPCWSTR wzAppBase,
    __out LPWSTR* psczBaFactoryAssemblyName
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczConfigPath = NULL;
    IXMLDOMDocument* pixdManifest = NULL;
    IXMLDOMNode* pixnHost = NULL;

    hr = PathConcat(wzAppBase, MBA_CONFIG_FILE_NAME, &sczConfigPath);
    ExitOnFailure(hr, "Failed to get the full path to the application configuration file.");

    hr = XmlInitialize();
    ExitOnFailure(hr, "Failed to initialize XML.");

    hr = XmlLoadDocumentFromFile(sczConfigPath, &pixdManifest);
    ExitOnFailure(hr, "Failed to load bootstrapper config file from path: %ls", sczConfigPath);

    hr = XmlSelectSingleNode(pixdManifest, L"/configuration/wix.bootstrapper/host", &pixnHost);
    ExitOnFailure(hr, "Failed to get startup element.");

    if (S_FALSE == hr)
    {
        hr = E_NOTFOUND;
        ExitOnRootFailure(hr, "Failed to find wix.bootstrapper/host element in bootstrapper application config.");
    }

    hr = XmlGetAttributeEx(pixnHost, L"assemblyName", psczBaFactoryAssemblyName);
    ExitOnFailure(hr, "Failed to get host/@assemblyName.");

LExit:
    ReleaseObject(pixnHost);
    ReleaseObject(pixdManifest);

    XmlUninitialize();

    return hr;
}

// Creates the bootstrapper app and returns it for the engine.
static HRESULT CreateManagedBootstrapperApplication(
    __in LPCWSTR wzBaFactoryAssemblyName,
    __in DWORD dwDomainId,
    __in const BOOTSTRAPPER_CREATE_ARGS* pArgs,
    __inout BOOTSTRAPPER_CREATE_RESULTS* pResults
    )
{
    HRESULT hr = S_OK;
    IBootstrapperApplicationFactory* pAppFactory = NULL;

    hr = CreateManagedBootstrapperApplicationFactory(wzBaFactoryAssemblyName, dwDomainId, &pAppFactory);
    ExitOnFailure(hr, "Failed to create the factory to create the bootstrapper application.");

    hr = pAppFactory->Create(pArgs, pResults);
    ExitOnFailure(hr, "Failed to create the bootstrapper application.");

LExit:
    ReleaseNullObject(pAppFactory);

    return hr;
}

// Creates the app factory to create the managed app in the default AppDomain.
static HRESULT CreateManagedBootstrapperApplicationFactory(
    __in LPCWSTR wzBaFactoryAssemblyName,
    __in DWORD dwDomainId,
    __out IBootstrapperApplicationFactory** ppAppFactory
    )
{
    HRESULT hr = S_OK;
    PFNCREATEBAFACTORY pfnCreateBAFactory = NULL;

    hr = vpCLRHost->CreateDelegate(
        dwDomainId,
        MBA_ASSEMBLY_FULL_NAME,
        MBA_ENTRY_TYPE,
        MBA_STATIC_ENTRY_METHOD,
        reinterpret_cast<INT_PTR*>(&pfnCreateBAFactory));
    ExitOnRootFailure(hr, "Failed to create delegate in app domain.");

    *ppAppFactory = pfnCreateBAFactory(wzBaFactoryAssemblyName);

LExit:
    return hr;
}
