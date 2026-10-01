// Loads the proxy d3d11.dll from its own folder, then creates a device and a separate
// DXGI swapchain (as dgVoodoo does) and presents a few frames.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdio.h>
int main()
{
	HWND hwnd = CreateWindowW(L"STATIC", L"xrtest", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 640, 480, 0, 0, 0, 0);
	ID3D11Device* dev; ID3D11DeviceContext* ctx;
	HRESULT hr = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, D3D11_CREATE_DEVICE_SINGLETHREADED, 0, 0, D3D11_SDK_VERSION, &dev, 0, &ctx);
	printf("CreateDevice 0x%08lx\n", hr); if (FAILED(hr)) return 1;
	IDXGIFactory2* f; CreateDXGIFactory1(IID_PPV_ARGS(&f));
	DXGI_SWAP_CHAIN_DESC1 d = {}; d.Width = 640; d.Height = 480; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1;
	d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = 2; d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	IDXGISwapChain1* sc; hr = f->CreateSwapChainForHwnd(dev, hwnd, &d, 0, 0, &sc);
	printf("CreateSwapChainForHwnd 0x%08lx\n", hr); if (FAILED(hr)) return 1;
	for (int i = 0; i < 60; ++i) { ID3D11Texture2D* bb; sc->GetBuffer(0, IID_PPV_ARGS(&bb)); ID3D11RenderTargetView* rtv; dev->CreateRenderTargetView(bb, 0, &rtv);
		float c[4] = { i / 60.f, 0.2f, 0.4f, 1 }; ctx->ClearRenderTargetView(rtv, c); rtv->Release(); bb->Release();
		hr = (i & 1) ? sc->Present(1, 0) : sc->Present1(1, 0, &DXGI_PRESENT_PARAMETERS{}); if (FAILED(hr)) { printf("Present 0x%08lx\n", hr); return 1; } }
	printf("presented 60 frames\n");
	return 0;
}
