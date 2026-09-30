// Drives the proxy's stereo API the way cshell does: BeginStereoFrame, draw each eye into its
// half of the back buffer, SubmitStereo, Present.
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <stdio.h>
#include "../avp2xr_api.h"
int main()
{
	setvbuf(stdout, 0, _IONBF, 0);
	HWND hwnd = CreateWindowW(L"STATIC", L"stereotest", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 640, 480, 0, 0, 0, 0);
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

	int stereoFrames = 0;
	for (int i = 0; i < 300; ++i)
	{
		Avp2XrView views[2];
		int stereo = api->BeginStereoFrame(views);
		ID3D11Texture2D* bb; sc->GetBuffer(0, IID_PPV_ARGS(&bb)); ID3D11RenderTargetView* rtv; dev->CreateRenderTargetView(bb, 0, &rtv);
		float grey[4] = { 0.3f, 0.3f, 0.3f, 1 }, red[4] = { 0.8f, 0.1f, 0.1f, 1 }, blue[4] = { 0.1f, 0.1f, 0.8f, 1 };
		ctx->ClearRenderTargetView(rtv, grey);
		if (stereo)
		{
			D3D11_RECT l = { 0, 0, 2244, 2352 }, r = { 2244, 0, 4488, 2352 };
			ctx1->ClearView(rtv, red, &l, 1);
			ctx1->ClearView(rtv, blue, &r, 1);
			Avp2XrEyeSubmit eyes[2];
			for (int e = 0; e < 2; ++e)
			{
				float hx = -views[e].fov[0] > views[e].fov[1] ? -views[e].fov[0] : views[e].fov[1];
				float hy = views[e].fov[2] > -views[e].fov[3] ? views[e].fov[2] : -views[e].fov[3];
				eyes[e] = { { -hx, hx, hy, -hy }, { e * 0.5f, 0, 0.5f, 1 } };
			}
			if (stereoFrames++ == 0)
				printf("first stereo frame: left eye pos (%.3f %.3f %.3f) fov L%.2f R%.2f U%.2f D%.2f, right eye pos (%.3f %.3f %.3f)\n",
					views[0].position[0], views[0].position[1], views[0].position[2], views[0].fov[0], views[0].fov[1], views[0].fov[2], views[0].fov[3],
					views[1].position[0], views[1].position[1], views[1].position[2]);
			Avp2XrInput in;
			static int lastAim = -1;
			if (api->GetInput(&in) && in.aimValid != lastAim)
			{
				lastAim = in.aimValid;
				printf("input: aim %s pos (%.2f %.2f %.2f), move %s (%.2f %.2f)\n", in.aimValid ? "tracked" : "untracked",
					in.aimPosition[0], in.aimPosition[1], in.aimPosition[2], in.moveValid ? "active" : "inactive", in.move[0], in.move[1]);
			}
			api->SubmitStereo(eyes, AVP2XR_HUD_FOLLOWS);
			rtv->Release(); bb->Release();
			sc->Present(1, 0);

			// HUD pass: black with a white crosshair in the middle and a green bar at the bottom
			sc->GetBuffer(0, IID_PPV_ARGS(&bb)); dev->CreateRenderTargetView(bb, 0, &rtv);
			float black[4] = { 0, 0, 0, 1 }, white[4] = { 1, 1, 1, 1 }, green[4] = { 0.1f, 0.9f, 0.2f, 1 };
			ctx->ClearRenderTargetView(rtv, black);
			D3D11_RECT h = { 2124, 1168, 2364, 1184 }, v = { 2236, 1056, 2252, 1296 }, bar = { 300, 2150, 1500, 2250 };
			ctx1->ClearView(rtv, white, &h, 1); ctx1->ClearView(rtv, white, &v, 1); ctx1->ClearView(rtv, green, &bar, 1);
		}
		rtv->Release(); bb->Release();
		sc->Present(1, 0);
	}
	printf("300 frames, %d stereo\n", stereoFrames);
	return 0;
}
