// DirectDraw 7 app for seeing how dgVoodoo stretches the game's 2D drawing to its forced
// resolution: every pixel's colour encodes its own position (R = x & 255, B = y & 255,
// G = (x >> 8) | (y >> 8) << 4). Run with AVP2XR_DUMP set and decode the proxy's dump.
// Usage: ddpattern blt|quad|text [offset] [frames]
//   text: aliased GDI text and lines, blitted, to look at what the headset gets
//   blt:  an offscreen surface blitted to the back buffer (plain surfaces)
//   quad: a pre-transformed textured quad, point sampled (optimized surfaces); the quad's corners
//         are at -offset (default 0.5, texel centres on pixel centres)
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../avp2xr_api.h"

static const int W = 1280, H = 960;

static void FillPattern(IDirectDrawSurface7* s)
{
	DDSURFACEDESC2 sd = { sizeof(sd) };
	if (FAILED(s->Lock(nullptr, &sd, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr)))
	{
		printf("Lock failed\n");
		return;
	}
	for (int y = 0; y < H && y < (int)sd.dwHeight; ++y)
	{
		DWORD* row = (DWORD*)((char*)sd.lpSurface + y * sd.lPitch);
		for (int x = 0; x < W && x < (int)sd.dwWidth; ++x)
		{
			DWORD r = x & 255, g = (x >> 8) | ((y >> 8) << 4), b = y & 255;
			row[x] = 0xff000000 | (r << 16) | (g << 8) | b;
		}
	}
	s->Unlock(nullptr);
}

int main(int argc, char** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);
	bool quad = argc > 1 && !strcmp(argv[1], "quad");
	bool text = argc > 1 && !strcmp(argv[1], "text");
	float offset = argc > 2 ? (float)atof(argv[2]) : 0.5f;
	int frames = argc > 3 ? atoi(argv[3]) : 120;

	HWND hwnd = CreateWindowExA(0, "STATIC", "ddpattern", WS_POPUP | WS_VISIBLE, 0, 0, W, H, 0, 0, 0, 0);
	IDirectDraw7* dd = nullptr;
	HRESULT hr = DirectDrawCreateEx(nullptr, (void**)&dd, IID_IDirectDraw7, nullptr);
	if (FAILED(hr)) { printf("DirectDrawCreateEx 0x%08lx\n", hr); return 1; }
	if (FAILED(hr = dd->SetCooperativeLevel(hwnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_FPUSETUP))) { printf("SetCooperativeLevel 0x%08lx\n", hr); return 1; }
	if (FAILED(hr = dd->SetDisplayMode(W, H, 32, 0, 0))) { printf("SetDisplayMode 0x%08lx\n", hr); return 1; }

	DDSURFACEDESC2 sd = { sizeof(sd) };
	sd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_3DDEVICE;
	sd.dwBackBufferCount = 1;
	IDirectDrawSurface7* primary = nullptr;
	IDirectDrawSurface7* back = nullptr;
	if (FAILED(hr = dd->CreateSurface(&sd, &primary, nullptr))) { printf("CreateSurface 0x%08lx\n", hr); return 1; }
	DDSCAPS2 caps = { DDSCAPS_BACKBUFFER };
	if (FAILED(hr = primary->GetAttachedSurface(&caps, &back))) { printf("GetAttachedSurface 0x%08lx\n", hr); return 1; }

	DDPIXELFORMAT pf = { sizeof(pf) };
	pf.dwFlags = DDPF_RGB;
	pf.dwRGBBitCount = 32;
	pf.dwRBitMask = 0xff0000;
	pf.dwGBitMask = 0xff00;
	pf.dwBBitMask = 0xff;

	IDirectDrawSurface7* image = nullptr;
	IDirect3D7* d3d = nullptr;
	IDirect3DDevice7* dev = nullptr;
	const int TW = 2048, TH = 1024;
	if (!quad)
	{
		DDSURFACEDESC2 od = { sizeof(od) };
		od.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
		od.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
		od.dwWidth = W;
		od.dwHeight = H;
		od.ddpfPixelFormat = pf;
		if (FAILED(hr = dd->CreateSurface(&od, &image, nullptr))) { printf("offscreen CreateSurface 0x%08lx\n", hr); return 1; }
	}
	else
	{
		if (FAILED(hr = dd->QueryInterface(IID_IDirect3D7, (void**)&d3d))) { printf("IDirect3D7 0x%08lx\n", hr); return 1; }
		if (FAILED(hr = d3d->CreateDevice(IID_IDirect3DHALDevice, back, &dev))) { printf("CreateDevice 0x%08lx\n", hr); return 1; }
		DDSURFACEDESC2 td = { sizeof(td) };
		td.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
		td.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
		td.ddsCaps.dwCaps2 = DDSCAPS2_TEXTUREMANAGE;
		td.dwWidth = TW;
		td.dwHeight = TH;
		td.ddpfPixelFormat = pf;
		if (FAILED(hr = dd->CreateSurface(&td, &image, nullptr))) { printf("texture CreateSurface 0x%08lx\n", hr); return 1; }
	}
	if (!text)
		FillPattern(image);
	else
	{
		DDBLTFX fx = { sizeof(fx) };
		image->Blt(nullptr, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
		HDC dc;
		if (SUCCEEDED(image->GetDC(&dc)))
		{
			SetBkMode(dc, TRANSPARENT);
			int y = 200;
			const int sizes[] = { 11, 13, 16, 20, 26 };
			for (int i = 0; i < 5; ++i)
			{
				HFONT font = CreateFontA(-sizes[i], 0, 0, 0, i == 2 ? FW_BOLD : FW_NORMAL, 0, 0, 0, ANSI_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, "Arial");
				HGDIOBJ old = SelectObject(dc, font);
				SetTextColor(dc, i & 1 ? RGB(255, 220, 60) : RGB(230, 230, 230));
				const char* line = "SINGLE PLAYER  Multiplayer  Options  Load Game  0123456789 // WMAVXKz";
				TextOutA(dc, 400, y, line, (int)strlen(line));
				y += sizes[i] + 8;
				SelectObject(dc, old);
				DeleteObject(font);
			}
			HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
			HGDIOBJ oldPen = SelectObject(dc, pen);
			for (int k = 0; k < 6; ++k)
			{
				MoveToEx(dc, 400 + k * 60, 420, nullptr);
				LineTo(dc, 440 + k * 60 + k * 8, 520);
			}
			SelectObject(dc, oldPen);
			DeleteObject(pen);
			image->ReleaseDC(dc);
		}
	}

	for (int i = 0; i < frames; ++i)
	{
		// The game (cshell) tells the proxy its resolution
		static bool told;
		if (!told)
		{
			auto getApi = (PFN_avp2xr_GetApi)GetProcAddress(GetModuleHandleW(L"d3d11.dll"), "avp2xr_GetApi");
			if (getApi)
			{
				getApi()->SetGameResolution(W, H);
				told = true;
			}
		}
		DDBLTFX fx = { sizeof(fx) };
		back->Blt(nullptr, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
		if (!quad)
		{
			RECT r = { 0, 0, W, H };
			back->Blt(&r, image, &r, DDBLT_WAIT, nullptr);
		}
		else
		{
			dev->BeginScene();
			dev->SetTexture(0, image);
			dev->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_POINT);
			dev->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_POINT);
			dev->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTFP_NONE);
			dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			dev->SetRenderState(D3DRENDERSTATE_ZENABLE, FALSE);
			dev->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
			dev->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
			float u = (float)W / TW, v = (float)H / TH;
			float x0 = -offset, y0 = -offset, x1 = W - offset, y1 = H - offset;
			D3DTLVERTEX q[4];
			memset(q, 0, sizeof(q));
			q[0].sx = x0; q[0].sy = y0; q[0].tu = 0; q[0].tv = 0;
			q[1].sx = x1; q[1].sy = y0; q[1].tu = u; q[1].tv = 0;
			q[2].sx = x0; q[2].sy = y1; q[2].tu = 0; q[2].tv = v;
			q[3].sx = x1; q[3].sy = y1; q[3].tu = u; q[3].tv = v;
			for (int k = 0; k < 4; ++k) { q[k].rhw = 1; q[k].color = 0xffffffff; }
			dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, q, 4, 0);
			dev->EndScene();
		}
		primary->Flip(nullptr, DDFLIP_WAIT);
	}
	printf("%s: %d frames\n", quad ? "quad" : "blt", frames);
	if (dev) dev->Release();
	if (d3d) d3d->Release();
	image->Release();
	back->Release();
	primary->Release();
	dd->RestoreDisplayMode();
	dd->Release();
	return 0;
}
