#include "pch.h"
#include "dllmain.h"
#include "Detours.h"

#define _CRT_SECURE_NO_DEPRECATE
#pragma warning (disable : 4996)

#ifdef _DEBUG
#pragma comment(lib, "detours.d.lib")
#else
#pragma comment(lib, "detours.r.lib")
#endif // DEBUG

PFN_CREATE_DXGI_FACTORY createDxgiFactory;
PFN_CREATE_DXGI_FACTORY createDxgiFactory1;
PFN_CREATE_DXGI_FACTORY_2 createDxgiFactory2;
PFN_DECLARE_ADAPTER_REMOVAL_SUPPORT declareAdapterRemovalSupport;
PFN_GET_DEBUG_INTERFACE getDebugInterface;

decltype(&IDXGIAdapter::GetDesc) ptrGetDesc = nullptr;
decltype(&IDXGIAdapter1::GetDesc1) ptrGetDesc1 = nullptr;
decltype(&IDXGIAdapter2::GetDesc2) ptrGetDesc2 = nullptr;
decltype(&IDXGIAdapter4::GetDesc3) ptrGetDesc3 = nullptr;

decltype(&IDXGIFactory::EnumAdapters) ptrEnumAdapters = nullptr;
decltype(&IDXGIFactory1::EnumAdapters1) ptrEnumAdapters1 = nullptr;
decltype(&IDXGIFactory4::EnumAdapterByLuid) ptrEnumAdapterByLuid = nullptr;
decltype(&IDXGIFactory6::EnumAdapterByGpuPreference) ptrEnumAdapterByGpuPreference = nullptr;

void loadOriginalDXGI()
{
	originalDXGI = LoadLibrary(".\\dxgi.o.dll");

	if (!originalDXGI)
	{
		char dllpath[MAX_PATH];
		GetSystemDirectory(dllpath, MAX_PATH);
		strcat(dllpath, "\\dxgi.dll");
		LOG(dllpath);
		originalDXGI = LoadLibrary(dllpath);
	}

	if (!originalDXGI)
	{
		LOG("loadOriginalDXGI: no originalDXGI");
		return;
	}

	createDxgiFactory = (PFN_CREATE_DXGI_FACTORY)GetProcAddress(originalDXGI, "CreateDXGIFactory");
	createDxgiFactory1 = (PFN_CREATE_DXGI_FACTORY)GetProcAddress(originalDXGI, "CreateDXGIFactory1");
	createDxgiFactory2 = (PFN_CREATE_DXGI_FACTORY_2)GetProcAddress(originalDXGI, "CreateDXGIFactory2");
	declareAdapterRemovalSupport = (PFN_DECLARE_ADAPTER_REMOVAL_SUPPORT)GetProcAddress(originalDXGI, "DXGIDeclareAdapterRemovalSupport");
	getDebugInterface = (PFN_GET_DEBUG_INTERFACE)GetProcAddress(originalDXGI, "DXGIGetDebugInterface1");
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		prepareOfs(".\\dxgi-proxy.log");
		LOG("dxgi-proxy DLL_PROCESS_ATTACH");
		loadOriginalDXGI();
		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		LOG("dxgi-proxy DLL_PROCESS_DETACH");

		if (originalDXGI)
			FreeLibrary(originalDXGI);

		closeOfs();

		break;
	}

	return TRUE;
}

#pragma region Adapter

const std::string xessName = "xessD3D12BuildPipelines";

HRESULT WINAPI detGetDesc3(IDXGIAdapter4* This, /* [annotation][out] */ _Out_  DXGI_ADAPTER_DESC3* pDesc)
{
	LOG("IDXGIAdapter4.GetDesc3");

	// Walk the call stack to find the DLL that is calling the hooked function
	void* callers[100];
	unsigned short frames = CaptureStackBackTrace(0, 100, callers, NULL);
	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);
	SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	bool xess = false;

	for (unsigned int i = 0; i < frames; i++)
	{
		SymFromAddr(process, (DWORD64)callers[i], 0, symbol);

		// Get module information for the current stack frame
		DWORD moduleBase = SymGetModuleBase64(process, symbol->Address);

		auto sn = std::string(symbol->Name);
		LOG("detGetDesc3: " + sn);
		if (sn == xessName)
		{
			xess = true;
			break;
		}
	}

	free(symbol);
	SymCleanup(process);

	auto result = (This->*ptrGetDesc3)(pDesc);

	if (result == S_OK && pDesc->VendorId != 0x1414)
	{
		if (xess)
		{
			LOG("IDXGIAdapter4.GetDesc3 Spoofing card as Intel");
			pDesc->VendorId = 0x8086;
			pDesc->DeviceId = 0x56a1;

			std::wstring name(L"Intel(R) Arc(TM) A750 Graphics");
			const wchar_t* szName = name.c_str();
			std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
			std::memcpy(pDesc->Description, szName, 61);
		}
		else
		{
			LOG("IDXGIAdapter4.GetDesc3 Spoofing card as NVidia");
			pDesc->VendorId = 0x10de;
			pDesc->DeviceId = 0x2684;

			std::wstring name(L"NVIDIA GeForce RTX 4090");
			const wchar_t* szName = name.c_str();
			std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
			std::memcpy(pDesc->Description, szName, 54);
		}
	}

	AttachToAdapter(This);

	LOG("IDXGIAdapter4.GetDesc3 result: " + int_to_hex(result));

	return result;
}


HRESULT WINAPI detGetDesc2(IDXGIAdapter2* This, /* [annotation][out] */ _Out_  DXGI_ADAPTER_DESC2* pDesc)
{
	LOG("IDXGIAdapter2.GetDesc2");

	// Walk the call stack to find the DLL that is calling the hooked function
	void* callers[100];
	unsigned short frames = CaptureStackBackTrace(0, 100, callers, NULL);
	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);
	SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	bool xess = false;

	for (unsigned int i = 0; i < frames; i++)
	{
		SymFromAddr(process, (DWORD64)callers[i], 0, symbol);

		// Get module information for the current stack frame
		DWORD moduleBase = SymGetModuleBase64(process, symbol->Address);

		auto sn = std::string(symbol->Name);
		LOG("detGetDesc2: " + sn);
		if (sn == xessName)
		{
			xess = true;
			break;
		}
	}

	free(symbol);
	SymCleanup(process);

	auto result = (This->*ptrGetDesc2)(pDesc);

	if (result == S_OK && pDesc->VendorId != 0x1414)
	{
		if (xess)
		{
			LOG("IDXGIAdapter4.GetDesc3 Spoofing card as Intel");
			pDesc->VendorId = 0x8086;
			pDesc->DeviceId = 0x56a1;

			std::wstring name(L"Intel(R) Arc(TM) A750 Graphics");
			const wchar_t* szName = name.c_str();
			std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
			std::memcpy(pDesc->Description, szName, 61);
		}
		else
		{
			LOG("IDXGIAdapter4.GetDesc3 Spoofing card as NVidia");
			pDesc->VendorId = 0x10de;
			pDesc->DeviceId = 0x2684;

			std::wstring name(L"NVIDIA GeForce RTX 4090");
			const wchar_t* szName = name.c_str();
			std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
			std::memcpy(pDesc->Description, szName, 54);
		}
	}

	AttachToAdapter(This);

	LOG("IDXGIAdapter2.GetDesc2 result: " + int_to_hex(result));

	return result;
}

HRESULT WINAPI detGetDesc1(IDXGIAdapter1* This, /* [annotation][out] */ _Out_  DXGI_ADAPTER_DESC1* pDesc)
{
	LOG("IDXGIAdapter1.GetDesc1");

	// Walk the call stack to find the DLL that is calling the hooked function
	void* callers[100];
	unsigned short frames = CaptureStackBackTrace(0, 100, callers, NULL);
	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);
	SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	bool xess = false;

	for (unsigned int i = 0; i < frames; i++)
	{
		SymFromAddr(process, (DWORD64)callers[i], 0, symbol);

		// Get module information for the current stack frame
		DWORD moduleBase = SymGetModuleBase64(process, symbol->Address);

		auto sn = std::string(symbol->Name);
		LOG("detGetDesc1: " + sn);
		if (sn == xessName)
		{
			xess = true;
			break;
		}
	}

	free(symbol);
	SymCleanup(process);

	auto result = (This->*ptrGetDesc1)(pDesc);

	if (result == S_OK && pDesc->VendorId != 0x1414)
	{
		if (xess)
		{
			LOG("IDXGIAdapter4.GetDesc3 Spoofing card as Intel");
			pDesc->VendorId = 0x8086;
			pDesc->DeviceId = 0x56a1;

			std::wstring name(L"Intel(R) Arc(TM) A750 Graphics");
			const wchar_t* szName = name.c_str();
			std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
			std::memcpy(pDesc->Description, szName, 61);
		}
		else
		{
			LOG("IDXGIAdapter4.GetDesc3 Spoofing card as NVidia");
			pDesc->VendorId = 0x10de;
			pDesc->DeviceId = 0x2684;

			std::wstring name(L"NVIDIA GeForce RTX 4090");
			const wchar_t* szName = name.c_str();
			std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
			std::memcpy(pDesc->Description, szName, 54);
		}
	}

	AttachToAdapter(This);

	LOG("IDXGIAdapter1.GetDesc1 result: " + int_to_hex(result));

	return result;
}


HRESULT WINAPI detGetDesc(IDXGIAdapter* This, /* [annotation][out] */ _Out_  DXGI_ADAPTER_DESC* pDesc)
{
	LOG("IDXGIAdapter.GetDesc");

	// Walk the call stack to find the DLL that is calling the hooked function
	void* callers[100];
	unsigned short frames = CaptureStackBackTrace(0, 100, callers, NULL);
	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);
	SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	bool xess = false;

	for (unsigned int i = 0; i < frames; i++)
	{
		SymFromAddr(process, (DWORD64)callers[i], 0, symbol);

		// Get module information for the current stack frame
		DWORD moduleBase = SymGetModuleBase64(process, symbol->Address);

		auto sn = std::string(symbol->Name);
		LOG("detGetDesc: " + sn);
		if (sn == xessName)
		{
			xess = true;
			break;
		}
	}

	free(symbol);
	SymCleanup(process);

	auto result = (This->*ptrGetDesc)(pDesc);

	if (result == S_OK && pDesc->VendorId != 0x1414)
	{
		if (xess)
		{
			LOG("IDXGIAdapter4.GetDesc3 Spoofing card as Intel");
			pDesc->VendorId = 0x8086;
			pDesc->DeviceId = 0x56a1;

			std::wstring name(L"Intel(R) Arc(TM) A750 Graphics");
			const wchar_t* szName = name.c_str();
			std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
			std::memcpy(pDesc->Description, szName, 61);
		}
		else
		{
			LOG("IDXGIAdapter4.GetDesc3 Spoofing card as NVidia");
			pDesc->VendorId = 0x10de;
			pDesc->DeviceId = 0x2684;

			std::wstring name(L"NVIDIA GeForce RTX 4090");
			const wchar_t* szName = name.c_str();
			std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
			std::memcpy(pDesc->Description, szName, 54);
		}
	}

	AttachToAdapter(This);

	LOG("IDXGIAdapter.GetDesc result: " + int_to_hex(result));

	return result;
}

#pragma endregion

#pragma region Factory

HRESULT WINAPI detEnumAdapterByGpuPreference(IDXGIFactory6* This, /* [annotation] */ _In_  UINT Adapter, /* [annotation] */ _In_  DXGI_GPU_PREFERENCE GpuPreference, /* [annotation] */ _In_  REFIID riid, /* [annotation] */ _COM_Outptr_  void** ppvAdapter)
{
	LOG("IDXGIFactory6.EnumAdapterByGpuPreference");

	AttachToFactory(This);

	IDXGIAdapter* adapter;
	auto result = (This->*ptrEnumAdapterByGpuPreference)(Adapter, GpuPreference, riid, (void**)&adapter);

	if (result == S_OK)
	{
		AttachToAdapter(adapter);
		*ppvAdapter = adapter;
	}

	LOG("IDXGIFactory6.EnumAdapterByGpuPreference result: " + int_to_hex(result));

	return result;
}

HRESULT WINAPI detEnumAdapterByLuid(IDXGIFactory4* This, /* [annotation] */ _In_  LUID AdapterLuid, /* [annotation] */ _In_  REFIID riid, /* [annotation] */ _COM_Outptr_  void** ppvAdapter)
{
	LOG("IDXGIFactory4.EnumAdapterByLuid");

	AttachToFactory(This);

	IDXGIAdapter* adapter;
	auto result = (This->*ptrEnumAdapterByLuid)(AdapterLuid, riid, (void**)&adapter);

	if (result == S_OK)
	{
		AttachToAdapter(adapter);
		*ppvAdapter = adapter;
	}

	LOG("IDXGIFactory4.EnumAdapterByLuid result: " + int_to_hex(result));

	return result;
}

HRESULT WINAPI detEnumAdapters1(IDXGIFactory1* This, /* [in] */ UINT Adapter, /* [annotation][out] */ _COM_Outptr_  IDXGIAdapter1** ppAdapter)
{
	LOG("IDXGIFactory1.EnumAdapters1 Adapter: " + std::to_string(Adapter));

	AttachToFactory(This);

	IDXGIAdapter1* adapter;
	auto result = (This->*ptrEnumAdapters1)(Adapter, &adapter);

	if (result == S_OK)
	{
		AttachToAdapter(adapter);
		*ppAdapter = adapter;
	}

	LOG("IDXGIFactory1.EnumAdapters1 result: " + int_to_hex(result));

	return result;
}

HRESULT WINAPI detEnumAdapters(IDXGIFactory* This, /* [in] */ UINT Adapter, /* [annotation][out] */ _COM_Outptr_  IDXGIAdapter** ppAdapter)
{
	LOG("IDXGIFactory.EnumAdapters Adapter: " + std::to_string(Adapter));

	AttachToFactory(This);

	IDXGIAdapter* adapter;
	auto result = (This->*ptrEnumAdapters)(Adapter, &adapter);

	if (result == S_OK)
	{
		AttachToAdapter(adapter);
		*ppAdapter = adapter;
	}

	LOG("IDXGIFactory.EnumAdapters result: " + int_to_hex(result));

	return result;
}

#pragma endregion

#pragma region DXGI

HRESULT WINAPI CreateDXGIFactory(REFIID riid, _COM_Outptr_ void** ppFactory)
{
	LOG("CreateDXGIFactory");

	if (!originalDXGI)
	{
		LOG("CreateDXGIFactory: no originalDXGI");
		return E_NOINTERFACE;
	}

	IDXGIFactory* factory;
	HRESULT result = createDxgiFactory(riid, (void**)&factory);

	if (result == S_OK)
	{
		AttachToFactory(factory);
		*ppFactory = factory;

		LOG("CreateDXGIFactory: Factory created");
	}

	return result;
}

HRESULT WINAPI CreateDXGIFactory1(REFIID riid, _COM_Outptr_ void** ppFactory)
{
	LOG("CreateDXGIFactory1");

	if (!originalDXGI)
	{
		LOG("CreateDXGIFactory1: no originalDXGI");
		return E_NOINTERFACE;
	}

	IDXGIFactory1* factory1;
	HRESULT result = createDxgiFactory1(riid, (void**)&factory1);

	if (result == S_OK)
	{
		AttachToFactory(factory1);
		*ppFactory = factory1;

		LOG("CreateDXGIFactory1: Factory created");
	}

	return result;
}

HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, _COM_Outptr_ void** ppFactory)
{
	LOG("CreateDXGIFactory2: " + ToString(riid));

	if (!originalDXGI)
	{
		LOG("CreateDXGIFactory2: no originalDXGI");
		return E_NOINTERFACE;
	}

	IDXGIFactory* factory;
	HRESULT result = createDxgiFactory2(Flags, riid, (void**)&factory);

	if (result == S_OK)
	{
		AttachToFactory(factory);
		*ppFactory = factory;

		LOG("CreateDXGIFactory2: Factory created");
	}

	return result;
}

HRESULT WINAPI DXGIDeclareAdapterRemovalSupport()
{
	LOG("DXGIDeclareAdapterRemovalSupport");

	if (!originalDXGI)
	{
		LOG("DXGIDeclareAdapterRemovalSupport: no originalDXGI");
		return E_NOINTERFACE;
	}

	return declareAdapterRemovalSupport();
}

HRESULT WINAPI DXGIGetDebugInterface1(UINT Flags, REFIID riid, void** ppDebug)
{
	LOG("DXGIGetDebugInterface1");

	if (!originalDXGI)
	{
		LOG("DXGIGetDebugInterface1: no originalDXGI");
		return E_NOINTERFACE;
	}

	return getDebugInterface(Flags, riid, ppDebug);
}

#pragma endregion

#pragma region Hooks

void WINAPI AttachToAdapter(IUnknown* unkAdapter)
{
	IDXGIAdapter* adapter;
	if (ptrGetDesc == nullptr && unkAdapter->QueryInterface(__uuidof(IDXGIAdapter), (void**)&adapter) == S_OK)
	{
		LOG("AttachToAdapter IDXGIAdapter detGetDesc");
		*(uintptr_t*)&ptrGetDesc = Detours::X64::DetourClassVTable(*(uintptr_t*)adapter, &detGetDesc, 8);
		adapter->Release();
	}

	IDXGIAdapter1* adapter1;
	if (unkAdapter->QueryInterface(__uuidof(IDXGIAdapter1), (void**)&adapter1) == S_OK)
	{
		if (ptrGetDesc1 == nullptr)
		{
			*(uintptr_t*)&ptrGetDesc1 = Detours::X64::DetourClassVTable(*(uintptr_t*)adapter1, &detGetDesc1, 10);
			LOG("AttachToAdapter IDXGIAdapter1 detGetDesc1");
		}

		adapter1->Release();
	}

	IDXGIAdapter2* adapter2;
	if (unkAdapter->QueryInterface(__uuidof(IDXGIAdapter2), (void**)&adapter2) == S_OK)
	{
		if (ptrGetDesc2 == nullptr)
		{
			*(uintptr_t*)&ptrGetDesc2 = Detours::X64::DetourClassVTable(*(uintptr_t*)adapter2, &detGetDesc2, 11);
			LOG("AttachToAdapter IDXGIAdapter2 detGetDesc2");
		}

		adapter2->Release();
	}

	IDXGIAdapter4* adapter4;
	if (unkAdapter->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&adapter4) == S_OK)
	{
		if (ptrGetDesc3 == nullptr)
		{
			*(uintptr_t*)&ptrGetDesc3 = Detours::X64::DetourClassVTable(*(uintptr_t*)adapter4, &detGetDesc3, 18);
			LOG("AttachToAdapter IDXGIAdapter4 detGetDesc3");
		}

		adapter4->Release();
	}
}

void WINAPI AttachToFactory(IUnknown* unkFactory)
{
	IDXGIFactory* factory;
	if (ptrEnumAdapters == nullptr && unkFactory->QueryInterface(__uuidof(IDXGIFactory), (void**)&factory) == S_OK)
	{
		LOG("AttachToFactory IDXGIFactory");

		*(uintptr_t*)&ptrEnumAdapters = Detours::X64::DetourClassVTable(*(uintptr_t*)factory, &detEnumAdapters, 7);
		factory->Release();
	}

	IDXGIFactory1* factory1;
	if (unkFactory->QueryInterface(__uuidof(IDXGIFactory1), (void**)&factory1) == S_OK)
	{
		if (ptrEnumAdapters1 == nullptr)
		{
			*(uintptr_t*)&ptrEnumAdapters1 = Detours::X64::DetourClassVTable(*(uintptr_t*)factory1, &detEnumAdapters1, 12);
			LOG("AttachToFactory IDXGIFactory1 detEnumAdapters1");
		}

		factory1->Release();
	}

	IDXGIFactory4* factory4;
	if (unkFactory->QueryInterface(__uuidof(IDXGIFactory4), (void**)&factory4) == S_OK)
	{
		if (ptrEnumAdapterByLuid == nullptr)
		{
			*(uintptr_t*)&ptrEnumAdapterByLuid = Detours::X64::DetourClassVTable(*(uintptr_t*)factory4, &detEnumAdapterByLuid, 26);
			LOG("AttachToFactory IDXGIFactory4 detEnumAdapterByLuid");
		}

		factory4->Release();
	}

	IDXGIFactory6* factory6;
	if (unkFactory->QueryInterface(__uuidof(IDXGIFactory6), (void**)&factory6) == S_OK)
	{
		if (ptrEnumAdapterByGpuPreference == nullptr)
		{
			*(uintptr_t*)&ptrEnumAdapterByGpuPreference = Detours::X64::DetourClassVTable(*(uintptr_t*)factory6, &detEnumAdapterByGpuPreference, 29);
			LOG("AttachToFactory IDXGIFactory6 detEnumAdapterByGpuPreference");
		}

		factory6->Release();
	}
}

#pragma endregion 