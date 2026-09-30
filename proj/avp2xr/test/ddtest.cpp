// Minimal DirectDraw 7 fullscreen app, run under dgVoodoo to see how dgVoodoo's settings affect
// the D3D11 back buffer (the proxy logs its size). Usage: ddtest <width> <height> [frames]
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
	int w = argc > 1 ? atoi(argv[1]) : 1280;
	int h = argc > 2 ? atoi(argv[2]) : 960;
	int frames = argc > 3 ? atoi(argv[3]) : 60;

	HWND hwnd = CreateWindowExA(0, "STATIC", "ddtest", WS_POPUP | WS_VISIBLE, 0, 0, w, h, 0, 0, 0, 0);
	IDirectDraw7* dd = nullptr;
	HRESULT hr = DirectDrawCreateEx(nullptr, (void**)&dd, IID_IDirectDraw7, nullptr);
	if (FAILED(hr)) { printf("DirectDrawCreateEx 0x%08lx\n", hr); return 1; }
	if (FAILED(hr = dd->SetCooperativeLevel(hwnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN))) { printf("SetCooperativeLevel 0x%08lx\n", hr); return 1; }
	if (FAILED(hr = dd->SetDisplayMode(w, h, 32, 0, 0))) { printf("SetDisplayMode %dx%d 0x%08lx\n", w, h, hr); return 1; }

	DDSURFACEDESC2 sd = { sizeof(sd) };
	sd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_3DDEVICE;
	sd.dwBackBufferCount = 1;
	IDirectDrawSurface7* primary = nullptr;
	IDirectDrawSurface7* back = nullptr;
	if (FAILED(hr = dd->CreateSurface(&sd, &primary, nullptr))) { printf("CreateSurface 0x%08lx\n", hr); return 1; }
	DDSCAPS2 caps = { DDSCAPS_BACKBUFFER };
	if (FAILED(hr = primary->GetAttachedSurface(&caps, &back))) { printf("GetAttachedSurface 0x%08lx\n", hr); return 1; }

	for (int i = 0; i < frames; ++i)
	{
		DDBLTFX fx = { sizeof(fx) };
		fx.dwFillColor = (i * 4) & 0xff;
		back->Blt(nullptr, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
		primary->Flip(nullptr, DDFLIP_WAIT);
	}
	printf("%dx%d: %d flips ok\n", w, h, frames);
	back->Release();
	primary->Release();
	dd->RestoreDisplayMode();
	dd->Release();
	return 0;
}
