// Drives the proxy's flat path (menus): the game's screen fills the whole back buffer (as dgVoodoo
// stretches it) and nothing is submitted in stereo. The monitor copy should keep the game's 4:3:
// black bars left and right, the red band at the left edge of the picture and the green at the right.
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <stdio.h>
#include "../avp2xr_api.h"
int main()
{
	setvbuf(stdout, 0, _IONBF, 0);
	// Client area in the back buffer's shape, so the window doesn't stretch it again
	RECT r = { 0, 0, 748, 392 };
	AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
	HWND hwnd = CreateWindowW(L"STATIC", L"flattest", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, r.right - r.left, r.bottom - r.top, 0, 0, 0, 0);
	ID3D11Device* dev; ID3D11DeviceContext* ctx;
	if (FAILED(D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0, D3D11_SDK_VERSION, &dev, 0, &ctx))) return 1;
	ID3D11DeviceContext1* ctx1; ctx->QueryInterface(IID_PPV_ARGS(&ctx1));
	IDXGIFactory2* f; CreateDXGIFactory1(IID_PPV_ARGS(&f));
	DXGI_SWAP_CHAIN_DESC1 d = {}; d.Width = 4488; d.Height = 2352; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1;
	d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = 2; d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	IDXGISwapChain1* sc; if (FAILED(f->CreateSwapChainForHwnd(dev, hwnd, &d, 0, 0, &sc))) return 1;

	auto getApi = (PFN_avp2xr_GetApi)GetProcAddress(GetModuleHandleW(L"d3d11.dll"), "avp2xr_GetApi");
	if (!getApi) { printf("proxy API not found\n"); return 1; }
	const Avp2XrApi* api = getApi();
	printf("API version %d\n", api->version);
	api->SetGameResolution(1280, 960);

	for (int i = 0; i < 240; ++i)
	{
		ID3D11Texture2D* bb; sc->GetBuffer(0, IID_PPV_ARGS(&bb)); ID3D11RenderTargetView* rtv; dev->CreateRenderTargetView(bb, 0, &rtv);
		float blue[4] = { 0.1f, 0.1f, 0.8f, 1 }, red[4] = { 0.9f, 0.1f, 0.1f, 1 }, green[4] = { 0.1f, 0.9f, 0.2f, 1 };
		ctx->ClearRenderTargetView(rtv, blue);
		D3D11_RECT left = { 0, 0, 300, 2352 }, right = { 4188, 0, 4488, 2352 };
		ctx1->ClearView(rtv, red, &left, 1);
		ctx1->ClearView(rtv, green, &right, 1);
		rtv->Release(); bb->Release();
		sc->Present(1, 0);
		MSG m; while (PeekMessageW(&m, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&m);
	}
	printf("240 flat frames\n");
	return 0;
}
