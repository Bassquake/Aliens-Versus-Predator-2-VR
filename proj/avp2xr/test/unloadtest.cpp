// Mimics dgVoodoo: load the proxy, probe a device, FreeLibrary it, then present on a swapchain
// whose device comes straight from the system d3d11.dll. Crashed before the module was pinned.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdio.h>
static LONG WINAPI OnCrash(EXCEPTION_POINTERS* ep)
{
	void* at = ep->ExceptionRecord->ExceptionAddress;
	HMODULE m = 0; wchar_t name[MAX_PATH] = L"?";
	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)at, &m))
		GetModuleFileNameW(m, name, MAX_PATH);
	printf("CRASH 0x%08lx at %p = %ls+0x%lx (access %p)\n", ep->ExceptionRecord->ExceptionCode, at, name,
		(unsigned long)((char*)at - (char*)m), (void*)ep->ExceptionRecord->ExceptionInformation[1]);
	return EXCEPTION_EXECUTE_HANDLER;
}

int main()
{
	SetUnhandledExceptionFilter(OnCrash);
	setvbuf(stdout, 0, _IONBF, 0);
	HMODULE proxy = LoadLibraryW(L"d3d11.dll");  // resolves to the proxy next to this exe
	auto create = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(proxy, "D3D11CreateDevice");
	ID3D11Device* probe;
	if (FAILED(create(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0, D3D11_SDK_VERSION, &probe, 0, 0))) return 1;
	probe->Release();
	FreeLibrary(proxy);
	printf("proxy still loaded after FreeLibrary: %s\n", GetModuleHandleW(L"d3d11.dll") == proxy ? "yes" : "no");

	wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH); wcscat_s(sys, L"\\d3d11.dll");
	auto sysCreate = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(LoadLibraryW(sys), "D3D11CreateDevice");
	printf("system d3d11 %p (proxy %p), D3D11CreateDevice %p\n", GetModuleHandleW(sys), proxy, (void*)sysCreate);
	ID3D11Device* dev;
	if (FAILED(sysCreate(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0, D3D11_SDK_VERSION, &dev, 0, 0))) return 1;
	HWND hwnd = CreateWindowW(L"STATIC", L"unloadtest", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 320, 240, 0, 0, 0, 0);
	IDXGIFactory2* f; CreateDXGIFactory1(IID_PPV_ARGS(&f));
	DXGI_SWAP_CHAIN_DESC1 d = {}; d.Width = 320; d.Height = 240; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1;
	d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = 2; d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	IDXGISwapChain1* sc; if (FAILED(f->CreateSwapChainForHwnd(dev, hwnd, &d, 0, 0, &sc))) return 1;
	printf("swapchain created, presenting\n");
	for (int i = 0; i < 10; ++i) sc->Present(1, 0);
	printf("presented 10 frames after FreeLibrary\n");
	return 0;
}
