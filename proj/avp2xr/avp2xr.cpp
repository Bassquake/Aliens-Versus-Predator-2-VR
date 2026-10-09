// avp2xr: proxy d3d11.dll for AVP2 running under dgVoodoo2.
//
// dgVoodoo turns the game's D3D7 calls into D3D11 and loads d3d11.dll by name, so this DLL,
// placed in the game folder, is loaded in place of the system one. It forwards every export
// to the real %SystemRoot%\SysWOW64\d3d11.dll and hooks IDXGISwapChain::Present(1) through
// the shared DXGI vtable. On the first Present it creates an OpenXR session bound to
// dgVoodoo's D3D11 device, and after that it copies each finished frame into an OpenXR
// swapchain, which is shown as a flat quad in front of the viewer.
//
// The log goes to avp2xr.log next to this DLL. Optional settings are in avp2xr.ini.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <share.h>

#define XR_NO_PROTOTYPES
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "avp2xr_api.h"

// ---------------------------------------------------------------------------------------------
// Logging

static HMODULE g_hModule;
static wchar_t g_moduleDir[MAX_PATH];
static FILE* g_log;
static CRITICAL_SECTION g_logLock;

static void Log(const char* fmt, ...)
{
	EnterCriticalSection(&g_logLock);
	if (!g_log)
	{
		// Not the game folder: the game has no UAC manifest, so Windows would silently redirect
		// writes under Program Files to VirtualStore.
		wchar_t path[MAX_PATH] = L"";
		wchar_t dir[MAX_PATH];
		if (GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH))
		{
			wcscat_s(dir, L"\\avp2xr");
			CreateDirectoryW(dir, nullptr);
			swprintf_s(path, L"%s\\avp2xr.log", dir);
			g_log = _wfsopen(path, L"w", _SH_DENYWR);
		}
		if (!g_log)
		{
			GetTempPathW(MAX_PATH, dir);
			swprintf_s(path, L"%savp2xr.log", dir);
			g_log = _wfsopen(path, L"w", _SH_DENYWR);
		}
	}
	if (g_log)
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		fprintf(g_log, "%02d:%02d:%02d.%03d [%5lu] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentThreadId());
		va_list ap;
		va_start(ap, fmt);
		vfprintf(g_log, fmt, ap);
		va_end(ap);
		fputc('\n', g_log);
		fflush(g_log);
	}
	LeaveCriticalSection(&g_logLock);
}

// ---------------------------------------------------------------------------------------------
// Export forwarding to the system d3d11.dll

static HMODULE g_realD3D11;

#define D3D11_FORWARDS(X) \
	X(CreateDirect3D11DeviceFromDXGIDevice) \
	X(CreateDirect3D11SurfaceFromDXGISurface) \
	X(D3D11CoreCreateDevice) \
	X(D3D11CoreCreateLayeredDevice) \
	X(D3D11CoreGetLayeredDeviceSize) \
	X(D3D11CoreRegisterLayers) \
	X(D3D11CreateDeviceForD3D12) \
	X(D3D11On12CreateDevice) \
	X(D3DKMTCloseAdapter) \
	X(D3DKMTCreateAllocation) \
	X(D3DKMTCreateContext) \
	X(D3DKMTCreateDevice) \
	X(D3DKMTCreateSynchronizationObject) \
	X(D3DKMTDestroyAllocation) \
	X(D3DKMTDestroyContext) \
	X(D3DKMTDestroyDevice) \
	X(D3DKMTDestroySynchronizationObject) \
	X(D3DKMTEscape) \
	X(D3DKMTGetContextSchedulingPriority) \
	X(D3DKMTGetDeviceState) \
	X(D3DKMTGetDisplayModeList) \
	X(D3DKMTGetMultisampleMethodList) \
	X(D3DKMTGetRuntimeData) \
	X(D3DKMTGetSharedPrimaryHandle) \
	X(D3DKMTLock) \
	X(D3DKMTOpenAdapterFromHdc) \
	X(D3DKMTOpenResource) \
	X(D3DKMTPresent) \
	X(D3DKMTQueryAdapterInfo) \
	X(D3DKMTQueryAllocationResidency) \
	X(D3DKMTQueryResourceInfo) \
	X(D3DKMTRender) \
	X(D3DKMTSetAllocationPriority) \
	X(D3DKMTSetContextSchedulingPriority) \
	X(D3DKMTSetDisplayMode) \
	X(D3DKMTSetDisplayPrivateDriverFormat) \
	X(D3DKMTSetGammaRamp) \
	X(D3DKMTSetVidPnSourceOwner) \
	X(D3DKMTSignalSynchronizationObject) \
	X(D3DKMTUnlock) \
	X(D3DKMTWaitForSynchronizationObject) \
	X(D3DKMTWaitForVerticalBlankEvent) \
	X(D3DPerformance_BeginEvent) \
	X(D3DPerformance_EndEvent) \
	X(D3DPerformance_GetStatus) \
	X(D3DPerformance_SetMarker) \
	X(EnableFeatureLevelUpgrade) \
	X(OpenAdapter10) \
	X(OpenAdapter10_2)

// Each forwarder is a bare jmp through a pointer, so the arguments and calling convention
// pass through untouched.
#define DEFINE_FORWARD(name) \
	static FARPROC fwd_##name; \
	extern "C" __declspec(naked) void Fwd_##name() { __asm { jmp dword ptr [fwd_##name] } }
D3D11_FORWARDS(DEFINE_FORWARD)

static PFN_D3D11_CREATE_DEVICE g_realCreateDevice;
static PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN g_realCreateDeviceAndSwapChain;

static bool LoadRealD3D11()
{
	wchar_t path[MAX_PATH];
	GetSystemDirectoryW(path, MAX_PATH);  // System32, which WOW64 redirects to SysWOW64
	wcscat_s(path, L"\\d3d11.dll");
	g_realD3D11 = LoadLibraryW(path);
	if (!g_realD3D11)
		return false;

#define RESOLVE_FORWARD(name) fwd_##name = GetProcAddress(g_realD3D11, #name);
	D3D11_FORWARDS(RESOLVE_FORWARD)
	g_realCreateDevice = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(g_realD3D11, "D3D11CreateDevice");
	g_realCreateDeviceAndSwapChain = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(g_realD3D11, "D3D11CreateDeviceAndSwapChain");
	return g_realCreateDevice && g_realCreateDeviceAndSwapChain;
}

// ---------------------------------------------------------------------------------------------
// Settings

static struct
{
	// Flat virtual screen (menus, loading screens)
	float distance;     // metres in front of the starting head position
	float width;        // quad width in metres; height follows the game's aspect ratio
	float height;       // vertical offset of the quad centre from eye level, in metres
	// HUD panel shown over the stereo view, same units
	float hudDistance;
	float hudWidth;
	float hudHeight;
	// Stereo frames shown on the monitor too: 0 = none, 1 = every frame (about 0.15 ms each),
	// N = every Nth
	int desktopMirror;
	// What the monitor shows of a stereo frame: 0 = the left eye, 1 = the right eye (either
	// cropped to fill the screen), 2 = both side by side
	int mirrorEye;
	// Seconds an input written "long ..." in the bindings must be held
	float longPressTime;
	// Resolution of the HUD and screen images as a multiple of the headset's pixel density where
	// they're shown (area-filtered from the game's pixels, so the compositor maps them about 1:1);
	// 0 = the game's resolution as is
	float panelScale;
	// When the game runs in a window (it can start windowed, e.g. without focus at launch), stretch
	// the window over its monitor, borderless, so the monitor still shows the game full size
	int mirrorFill;
	// Bring the game's window to the front when it first appears (launched from SteamVR or a
	// shortcut it can start behind others, so the keyboard doesn't reach it until alt-tab)
	int focusWindow;
	// AdjustHandsLive: the Adjust binding (hand adjust mode in the game) is only bound when yes, so its
	// inputs (by default left stick click + right B) otherwise just do their own actions
	int adjustHandsLive;
	// Gamma for the eye images in the headset: above 1 brightens the darker tones (shadows) while
	// black and white stay put; 1 = as the game draws them (no extra pass)
	float gamma;
} g_cfg = { 2.0f, 3.0f, 0.0f, 1.5f, 2.25f, 0.0f, 1, 0, 0.5f, 1.0f, 1, 1, 0, 1.0f };

static float IniFloat(const wchar_t* ini, const wchar_t* key, float def)
{
	wchar_t buf[64];
	GetPrivateProfileStringW(L"VR", key, L"", buf, 64, ini);
	return buf[0] ? (float)_wtof(buf) : def;
}

static void LoadSettings()
{
	wchar_t ini[MAX_PATH];
	swprintf_s(ini, L"%savp2xr.ini", g_moduleDir);
	g_cfg.distance = IniFloat(ini, L"ScreenDistance", g_cfg.distance);
	g_cfg.width = IniFloat(ini, L"ScreenWidth", g_cfg.width);
	g_cfg.height = IniFloat(ini, L"ScreenHeight", g_cfg.height);
	g_cfg.hudDistance = IniFloat(ini, L"HudDistance", g_cfg.hudDistance);
	g_cfg.hudWidth = IniFloat(ini, L"HudWidth", g_cfg.hudWidth);
	g_cfg.hudHeight = IniFloat(ini, L"HudHeight", g_cfg.hudHeight);
	g_cfg.desktopMirror = (int)GetPrivateProfileIntW(L"VR", L"DesktopMirror", g_cfg.desktopMirror, ini);
	g_cfg.longPressTime = IniFloat(ini, L"LongPressTime", g_cfg.longPressTime);
	g_cfg.panelScale = IniFloat(ini, L"PanelScale", g_cfg.panelScale);
	wchar_t fill[16];
	GetPrivateProfileStringW(L"VR", L"MirrorFillScreen", L"yes", fill, 16, ini);
	g_cfg.mirrorFill = !fill[0] || !wcschr(L"nN0fF", fill[0]);
	wchar_t focus[16];
	GetPrivateProfileStringW(L"VR", L"FocusGameWindow", L"yes", focus, 16, ini);
	g_cfg.focusWindow = !focus[0] || !wcschr(L"nN0fF", focus[0]);
	wchar_t eye[16];
	GetPrivateProfileStringW(L"VR", L"MirrorEye", L"left", eye, 16, ini);
	g_cfg.mirrorEye = !_wcsicmp(eye, L"right") ? 1 : !_wcsicmp(eye, L"both") ? 2 : 0;
	wchar_t adjust[16];
	GetPrivateProfileStringW(L"VR", L"AdjustHandsLive", L"no", adjust, 16, ini);
	g_cfg.adjustHandsLive = adjust[0] && !wcschr(L"nN0fF", adjust[0]);
	g_cfg.gamma = IniFloat(ini, L"Gamma", g_cfg.gamma);
	if (g_cfg.gamma < 0.5f) g_cfg.gamma = 0.5f;
	if (g_cfg.gamma > 3.0f) g_cfg.gamma = 3.0f;
	Log("Settings: ScreenDistance=%.2f ScreenWidth=%.2f ScreenHeight=%.2f HudDistance=%.2f HudWidth=%.2f HudHeight=%.2f DesktopMirror=%d MirrorEye=%s PanelScale=%.2f MirrorFillScreen=%s FocusGameWindow=%s AdjustHandsLive=%s Gamma=%.2f",
		g_cfg.distance, g_cfg.width, g_cfg.height, g_cfg.hudDistance, g_cfg.hudWidth, g_cfg.hudHeight, g_cfg.desktopMirror,
		g_cfg.mirrorEye == 1 ? "right" : g_cfg.mirrorEye == 2 ? "both" : "left", g_cfg.panelScale, g_cfg.mirrorFill ? "yes" : "no", g_cfg.focusWindow ? "yes" : "no",
		g_cfg.adjustHandsLive ? "yes" : "no", g_cfg.gamma);
}

// ---------------------------------------------------------------------------------------------
// OpenXR

#define XR_FUNCS(X) \
	X(xrDestroyInstance) \
	X(xrGetInstanceProperties) \
	X(xrResultToString) \
	X(xrGetSystem) \
	X(xrGetSystemProperties) \
	X(xrCreateSession) \
	X(xrDestroySession) \
	X(xrBeginSession) \
	X(xrEndSession) \
	X(xrPollEvent) \
	X(xrCreateReferenceSpace) \
	X(xrDestroySpace) \
	X(xrEnumerateSwapchainFormats) \
	X(xrCreateSwapchain) \
	X(xrDestroySwapchain) \
	X(xrEnumerateSwapchainImages) \
	X(xrAcquireSwapchainImage) \
	X(xrWaitSwapchainImage) \
	X(xrReleaseSwapchainImage) \
	X(xrWaitFrame) \
	X(xrBeginFrame) \
	X(xrEndFrame) \
	X(xrLocateViews) \
	X(xrEnumerateViewConfigurationViews) \
	X(xrStringToPath) \
	X(xrCreateActionSet) \
	X(xrCreateAction) \
	X(xrSuggestInteractionProfileBindings) \
	X(xrAttachSessionActionSets) \
	X(xrCreateActionSpace) \
	X(xrSyncActions) \
	X(xrGetActionStateVector2f) \
	X(xrGetActionStateBoolean) \
	X(xrGetCurrentInteractionProfile) \
	X(xrLocateSpace)

static PFN_xrGetInstanceProcAddr x_xrGetInstanceProcAddr;
static PFN_xrCreateInstance x_xrCreateInstance;
static PFN_xrGetD3D11GraphicsRequirementsKHR x_xrGetD3D11GraphicsRequirementsKHR;
#define DECLARE_XR(name) static PFN_##name x_##name;
XR_FUNCS(DECLARE_XR)

enum XrState { XRS_UNINIT, XRS_ACTIVE, XRS_DISABLED };
static XrState g_xrState = XRS_UNINIT;
static XrInstance g_instance = XR_NULL_HANDLE;
static XrSystemId g_systemId = XR_NULL_SYSTEM_ID;
static XrSession g_session = XR_NULL_HANDLE;
static XrSpace g_space = XR_NULL_HANDLE;      // LOCAL: the eyes and the flat screen
static XrSpace g_viewSpace = XR_NULL_HANDLE;  // VIEW: follows the head, for the HUD panel
static bool g_running;  // between xrBeginSession and xrEndSession
static ID3D11Device* g_device;
static ID3D11DeviceContext* g_context;

static XrSwapchain g_swapchain = XR_NULL_HANDLE;
static UINT g_scWidth, g_scHeight;
static DXGI_FORMAT g_scSourceFormat;  // back buffer format the swapchain was made for
static XrSwapchainImageD3D11KHR* g_scImages;
static uint32_t g_scImageCount;
static bool g_formatWarned;

// Frame state. A frame is begun either by the game (BeginStereoFrame) or by the Present hook,
// and always ended by the Present hook.
static bool g_frameBegun;
static XrFrameState g_frameState = { XR_TYPE_FRAME_STATE };
static XrView g_views[2];
static uint32_t g_recommendedEyeWidth;  // pixels across the eye image the runtime asks for
static bool g_viewsValid;          // g_views were located for the begun frame
static bool g_stereoSubmitted;     // the game rendered both eyes for the begun frame
static bool g_hudFollows;          // ...and will flip a HUD-only image right after the eyes
static bool g_hudNext;             // the next Present is that HUD image
static Avp2XrEyeSubmit g_eyeSubmit[2];
static Avp2XrCrosshair g_crosshairs[2];  // for the begun frame; drawn into the eyes at their flip
static unsigned int* g_crosshairPixels;  // the latest SetCrosshairImage
static int g_crosshairWidth, g_crosshairHeight;
static bool g_crosshairImageNew;         // not in the texture yet
struct MarkerImage
{
	unsigned int* pixels;  // the latest SetMarkerImage
	int width, height;
	bool isNew;            // not in the texture yet
	ID3D11Texture2D* texture;
	ID3D11ShaderResourceView* srv;
};
static MarkerImage g_markerImages[AVP2XR_MAX_MARKER_IMAGES];
static Avp2XrMarker g_markers[AVP2XR_MAX_MARKERS];  // for the begun frame; drawn into the eyes at their flip
static int g_markerCount;

static int g_recenterCount;           // LOCAL space recenters that have taken effect (not reset with the session)
static XrTime g_recenterPendingTime;  // when a pending recenter takes effect; 0 if none

static void DestroyHud();
static void DestroyMirror();
static void DestroyCrosshair();
static void DestroyGamma();
static void ResetInput();
static void InitInput();
static bool g_waitingForHmd;  // xrGetSystem said no headset; retry later
static DWORD g_retryAt;

static const char* XrStr(XrResult r)
{
	static char buf[XR_MAX_RESULT_STRING_SIZE];
	if (g_instance && x_xrResultToString && XR_SUCCEEDED(x_xrResultToString(g_instance, r, buf)))
		return buf;
	sprintf_s(buf, "XrResult %d", (int)r);
	return buf;
}

#define XR_CHECK(call) \
	do { XrResult r_ = (call); if (XR_FAILED(r_)) { Log("%s failed: %s", #call, XrStr(r_)); return false; } } while (0)

static void DestroySwapchain()
{
	if (g_swapchain)
		x_xrDestroySwapchain(g_swapchain);
	g_swapchain = XR_NULL_HANDLE;
	delete[] g_scImages;
	g_scImages = nullptr;
	g_scImageCount = 0;
	g_scWidth = g_scHeight = 0;
}

// Tears everything down. After this, a later Present may start a new session on a new
// device unless `disable` is set.
static void XrShutdown(bool disable)
{
	if (g_instance)
	{
		DestroySwapchain();
		DestroyMirror();
		DestroyCrosshair();
		DestroyGamma();
		DestroyHud();
		if (g_space)
			x_xrDestroySpace(g_space);
		if (g_viewSpace)
			x_xrDestroySpace(g_viewSpace);
		if (g_session)
		{
			if (g_running)
				x_xrEndSession(g_session);
			x_xrDestroySession(g_session);
		}
		x_xrDestroyInstance(g_instance);
	}
	g_space = XR_NULL_HANDLE;
	g_viewSpace = XR_NULL_HANDLE;
	ResetInput();
	g_session = XR_NULL_HANDLE;
	g_instance = XR_NULL_HANDLE;
	g_running = false;
	g_frameBegun = g_viewsValid = g_stereoSubmitted = g_hudFollows = g_hudNext = false;
	if (g_context)
		g_context->Release();
	if (g_device)
		g_device->Release();
	g_context = nullptr;
	g_device = nullptr;
	g_xrState = disable ? XRS_DISABLED : XRS_UNINIT;
}

static bool LoadXrLoader()
{
	if (x_xrGetInstanceProcAddr)
		return true;
	wchar_t path[MAX_PATH];
	swprintf_s(path, L"%sopenxr_loader.dll", g_moduleDir);
	HMODULE loader = LoadLibraryW(path);
	if (!loader)
	{
		Log("Could not load %ls (error %lu)", path, GetLastError());
		return false;
	}
	x_xrGetInstanceProcAddr = (PFN_xrGetInstanceProcAddr)GetProcAddress(loader, "xrGetInstanceProcAddr");
	if (!x_xrGetInstanceProcAddr)
	{
		Log("openxr_loader.dll has no xrGetInstanceProcAddr");
		return false;
	}
	x_xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrCreateInstance", (PFN_xrVoidFunction*)&x_xrCreateInstance);
	return x_xrCreateInstance != nullptr;
}

static bool XrInit(ID3D11Device* device)
{
	if (!LoadXrLoader())
		return false;

	const char* extensions[] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME };
	XrInstanceCreateInfo ci = { XR_TYPE_INSTANCE_CREATE_INFO };
	strcpy_s(ci.applicationInfo.applicationName, "Aliens vs. Predator 2");
	ci.applicationInfo.applicationVersion = 1;
	strcpy_s(ci.applicationInfo.engineName, "LithTech");
	ci.applicationInfo.engineVersion = 1;
	ci.applicationInfo.apiVersion = XR_API_VERSION_1_0;
	ci.enabledExtensionCount = 1;
	ci.enabledExtensionNames = extensions;
	XrResult r = x_xrCreateInstance(&ci, &g_instance);
	// Steam sets XR_RUNTIME_JSON to the 64-bit SteamVR runtime for games it launches as VR, which
	// this 32-bit game can't load; without it the loader uses the registered 32-bit runtime.
	char runtimeJson[MAX_PATH] = "";
	GetEnvironmentVariableA("XR_RUNTIME_JSON", runtimeJson, MAX_PATH);
	if (r == XR_ERROR_RUNTIME_UNAVAILABLE && runtimeJson[0])
	{
		Log("XR_RUNTIME_JSON=%s can't be loaded by this 32-bit game (Steam sets it to the 64-bit SteamVR runtime); "
			"retrying with the registered 32-bit runtime", runtimeJson);
		SetEnvironmentVariableA("XR_RUNTIME_JSON", nullptr);
		runtimeJson[0] = 0;
		r = x_xrCreateInstance(&ci, &g_instance);
	}
	if (XR_FAILED(r))
	{
		Log("xrCreateInstance failed: %s. Is the OpenXR runtime (SteamVR / Meta) running, and is "
			"XR_KHR_D3D11_enable available to 32-bit apps?%s%s", XrStr(r), runtimeJson[0] ? " XR_RUNTIME_JSON=" : "", runtimeJson);
		g_instance = XR_NULL_HANDLE;
		return false;
	}

#define LOAD_XR(name) \
	if (XR_FAILED(x_xrGetInstanceProcAddr(g_instance, #name, (PFN_xrVoidFunction*)&x_##name))) { Log("Missing " #name); return false; }
	XR_FUNCS(LOAD_XR)
	LOAD_XR(xrGetD3D11GraphicsRequirementsKHR)

	XrInstanceProperties ip = { XR_TYPE_INSTANCE_PROPERTIES };
	if (XR_SUCCEEDED(x_xrGetInstanceProperties(g_instance, &ip)))
		Log("OpenXR runtime: %s %u.%u.%u", ip.runtimeName, XR_VERSION_MAJOR(ip.runtimeVersion),
			XR_VERSION_MINOR(ip.runtimeVersion), XR_VERSION_PATCH(ip.runtimeVersion));

	XrSystemGetInfo sgi = { XR_TYPE_SYSTEM_GET_INFO };
	sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	r = x_xrGetSystem(g_instance, &sgi, &g_systemId);
	if (r == XR_ERROR_FORM_FACTOR_UNAVAILABLE)
	{
		// The spec's advice: the headset is asleep or unplugged, so keep polling.
		if (!g_waitingForHmd)
			Log("No headset available yet (asleep or not connected); retrying every 5 seconds");
		g_waitingForHmd = true;
		return false;
	}
	XR_CHECK(r);
	g_waitingForHmd = false;

	XrSystemProperties sp = { XR_TYPE_SYSTEM_PROPERTIES };
	if (XR_SUCCEEDED(x_xrGetSystemProperties(g_instance, g_systemId, &sp)))
		Log("HMD: %s", sp.systemName);

	// The side-by-side back buffer matches the headset best at (2 x width) x height of this.
	XrViewConfigurationView vcv[2] = { { XR_TYPE_VIEW_CONFIGURATION_VIEW }, { XR_TYPE_VIEW_CONFIGURATION_VIEW } };
	uint32_t viewCount = 0;
	if (XR_SUCCEEDED(x_xrEnumerateViewConfigurationViews(g_instance, g_systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewCount, vcv)) && viewCount == 2)
	{
		g_recommendedEyeWidth = vcv[0].recommendedImageRectWidth;
		Log("Recommended per-eye resolution %ux%u (max %ux%u); ideal dgVoodoo Resolution = h:%u, v:%u",
			vcv[0].recommendedImageRectWidth, vcv[0].recommendedImageRectHeight, vcv[0].maxImageRectWidth, vcv[0].maxImageRectHeight,
			vcv[0].recommendedImageRectWidth * 2, vcv[0].recommendedImageRectHeight);
	}

	// The runtime needs the device to be on the adapter the headset is attached to.
	XrGraphicsRequirementsD3D11KHR req = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
	XR_CHECK(x_xrGetD3D11GraphicsRequirementsKHR(g_instance, g_systemId, &req));
	IDXGIDevice* dxgiDevice = nullptr;
	IDXGIAdapter* adapter = nullptr;
	DXGI_ADAPTER_DESC ad = {};
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) && SUCCEEDED(dxgiDevice->GetAdapter(&adapter)))
		adapter->GetDesc(&ad);
	if (adapter)
		adapter->Release();
	if (dxgiDevice)
		dxgiDevice->Release();
	Log("dgVoodoo device: adapter \"%ls\", feature level 0x%x (runtime needs >= 0x%x)", ad.Description,
		device->GetFeatureLevel(), req.minFeatureLevel);
	if (memcmp(&ad.AdapterLuid, &req.adapterLuid, sizeof(LUID)) != 0)
	{
		Log("dgVoodoo's adapter is not the one the headset uses. Set Adapters in dgVoodoo.conf to the headset's GPU.");
		return false;
	}
	if (device->GetFeatureLevel() < req.minFeatureLevel)
	{
		Log("Feature level too low. Set OutputAPI = d3d11_fl11_0 in dgVoodoo.conf.");
		return false;
	}

	XrGraphicsBindingD3D11KHR binding = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
	binding.device = device;
	XrSessionCreateInfo sci = { XR_TYPE_SESSION_CREATE_INFO };
	sci.next = &binding;
	sci.systemId = g_systemId;
	XR_CHECK(x_xrCreateSession(g_instance, &sci, &g_session));

	XrReferenceSpaceCreateInfo rsci = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	rsci.poseInReferenceSpace.orientation.w = 1.0f;
	XR_CHECK(x_xrCreateReferenceSpace(g_session, &rsci, &g_space));
	rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	XR_CHECK(x_xrCreateReferenceSpace(g_session, &rsci, &g_viewSpace));

	device->AddRef();
	g_device = device;
	device->GetImmediateContext(&g_context);
	Log("OpenXR session created");
	InitInput();
	return true;
}

static const char* SessionStateName(XrSessionState s)
{
	static const char* names[] = { "UNKNOWN", "IDLE", "READY", "SYNCHRONIZED", "VISIBLE", "FOCUSED", "STOPPING", "LOSS_PENDING", "EXITING" };
	return (unsigned)s < sizeof(names) / sizeof(names[0]) ? names[s] : "?";
}

// Returns false if the session ended and XR has been shut down.
static void UpdateActiveProfile();

static bool PollEvents()
{
	XrEventDataBuffer ev = { XR_TYPE_EVENT_DATA_BUFFER };
	while (x_xrPollEvent(g_instance, &ev) == XR_SUCCESS)
	{
		if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
		{
			Log("OpenXR instance loss pending; VR output stopped");
			XrShutdown(true);
			return false;
		}
		if (ev.type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED)
			UpdateActiveProfile();
		if (ev.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING)
		{
			// The user held the Meta/system button (or reset the seated position): the runtime
			// is moving LOCAL to the current head pose. Tell the game so it re-anchors too.
			const XrEventDataReferenceSpaceChangePending& rc = *(const XrEventDataReferenceSpaceChangePending*)&ev;
			if (rc.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL)
			{
				// LOCAL only moves at changeTime; BeginXrFrame counts it once a frame is past that,
				// so the game re-anchors against the new space, not the old one.
				g_recenterPendingTime = rc.changeTime > 0 ? rc.changeTime : 1;
				Log("Recenter requested by the runtime");
			}
		}
		if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
		{
			const XrEventDataSessionStateChanged& sc = *(const XrEventDataSessionStateChanged*)&ev;
			Log("Session state -> %s", SessionStateName(sc.state));
			switch (sc.state)
			{
			case XR_SESSION_STATE_READY:
			{
				XrSessionBeginInfo bi = { XR_TYPE_SESSION_BEGIN_INFO };
				bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
				XrResult r = x_xrBeginSession(g_session, &bi);
				if (XR_FAILED(r))
					Log("xrBeginSession failed: %s", XrStr(r));
				else
					g_running = true;
				break;
			}
			case XR_SESSION_STATE_STOPPING:
				x_xrEndSession(g_session);
				g_running = false;
				break;
			case XR_SESSION_STATE_EXITING:
			case XR_SESSION_STATE_LOSS_PENDING:
				Log("Session ended; VR output stopped");
				XrShutdown(true);
				return false;
			default:
				break;
			}
		}
		ev.type = XR_TYPE_EVENT_DATA_BUFFER;
		ev.next = nullptr;
	}
	return true;
}

static DXGI_FORMAT SrgbOf(DXGI_FORMAT f)
{
	switch (f)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case DXGI_FORMAT_B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	case DXGI_FORMAT_B8G8R8X8_UNORM: return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
	default: return f;
	}
}

static DXGI_FORMAT TypelessOf(DXGI_FORMAT f)
{
	switch (f)
	{
	case DXGI_FORMAT_R8G8B8A8_TYPELESS: case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS: case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8A8_TYPELESS;
	case DXGI_FORMAT_B8G8R8X8_TYPELESS: case DXGI_FORMAT_B8G8R8X8_UNORM: case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8X8_TYPELESS;
	case DXGI_FORMAT_R10G10B10A2_TYPELESS: case DXGI_FORMAT_R10G10B10A2_UNORM:
		return DXGI_FORMAT_R10G10B10A2_TYPELESS;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS: case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_TYPELESS;
	default: return f;
	}
}

// Picks a runtime swapchain format that CopyResource can fill from `source`. The game writes
// gamma-encoded colour into a UNORM back buffer, so the sRGB twin is preferred: the compositor
// then decodes it correctly.
static DXGI_FORMAT ChooseSwapchainFormat(DXGI_FORMAT source)
{
	uint32_t count = 0;
	if (XR_FAILED(x_xrEnumerateSwapchainFormats(g_session, 0, &count, nullptr)) || !count)
		return DXGI_FORMAT_UNKNOWN;
	int64_t* formats = new int64_t[count];
	x_xrEnumerateSwapchainFormats(g_session, count, &count, formats);

	DXGI_FORMAT wanted[] = { SrgbOf(source), source };
	DXGI_FORMAT chosen = DXGI_FORMAT_UNKNOWN;
	for (int w = 0; w < 2 && chosen == DXGI_FORMAT_UNKNOWN; ++w)
		for (uint32_t i = 0; i < count; ++i)
			if (formats[i] == wanted[w]) { chosen = wanted[w]; break; }
	for (uint32_t i = 0; i < count && chosen == DXGI_FORMAT_UNKNOWN; ++i)
		if (TypelessOf((DXGI_FORMAT)formats[i]) == TypelessOf(source))
			chosen = (DXGI_FORMAT)formats[i];

	if (chosen == DXGI_FORMAT_UNKNOWN && !g_formatWarned)
	{
		g_formatWarned = true;
		Log("No runtime swapchain format is copy-compatible with back buffer format %d. Runtime offers:", source);
		for (uint32_t i = 0; i < count; ++i)
			Log("  %d", (int)formats[i]);
	}
	delete[] formats;
	return chosen;
}

static bool EnsureSwapchain(const D3D11_TEXTURE2D_DESC& bb)
{
	if (g_swapchain && g_scWidth == bb.Width && g_scHeight == bb.Height && g_scSourceFormat == bb.Format)
		return true;
	DestroySwapchain();

	DXGI_FORMAT format = ChooseSwapchainFormat(bb.Format);
	if (format == DXGI_FORMAT_UNKNOWN)
		return false;

	XrSwapchainCreateInfo ci = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
	ci.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
	ci.format = format;
	ci.sampleCount = 1;
	ci.width = bb.Width;
	ci.height = bb.Height;
	ci.faceCount = 1;
	ci.arraySize = 1;
	ci.mipCount = 1;
	XR_CHECK(x_xrCreateSwapchain(g_session, &ci, &g_swapchain));

	XR_CHECK(x_xrEnumerateSwapchainImages(g_swapchain, 0, &g_scImageCount, nullptr));
	g_scImages = new XrSwapchainImageD3D11KHR[g_scImageCount];
	for (uint32_t i = 0; i < g_scImageCount; ++i)
	{
		g_scImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
		g_scImages[i].next = nullptr;
	}
	XR_CHECK(x_xrEnumerateSwapchainImages(g_swapchain, g_scImageCount, &g_scImageCount, (XrSwapchainImageBaseHeader*)g_scImages));

	g_scWidth = bb.Width;
	g_scHeight = bb.Height;
	g_scSourceFormat = bb.Format;
	Log("XR swapchain %ux%u, back buffer format %d -> swapchain format %d, %u images",
		bb.Width, bb.Height, bb.Format, format, g_scImageCount);
	return true;
}

// Copies the back buffer (which still holds the finished frame, since this runs before the
// real Present) into the next XR swapchain image.
static bool CopyFrame(IDXGISwapChain* sc)
{
	ID3D11Texture2D* backBuffer = nullptr;
	if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
		return false;
	D3D11_TEXTURE2D_DESC bb;
	backBuffer->GetDesc(&bb);

	bool ok = false;
	if (EnsureSwapchain(bb))
	{
		uint32_t index;
		XrSwapchainImageAcquireInfo ai = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
		XrSwapchainImageWaitInfo wi = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
		wi.timeout = XR_INFINITE_DURATION;
		if (XR_SUCCEEDED(x_xrAcquireSwapchainImage(g_swapchain, &ai, &index)) &&
			XR_SUCCEEDED(x_xrWaitSwapchainImage(g_swapchain, &wi)))
		{
			ID3D11Texture2D* dst = g_scImages[index].texture;
			if (bb.SampleDesc.Count > 1)
			{
				D3D11_TEXTURE2D_DESC dd;
				dst->GetDesc(&dd);
				g_context->ResolveSubresource(dst, 0, backBuffer, 0, dd.Format);
			}
			else
				g_context->CopyResource(dst, backBuffer);
			XrSwapchainImageReleaseInfo ri = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
			ok = XR_SUCCEEDED(x_xrReleaseSwapchainImage(g_swapchain, &ri));
		}
	}
	backBuffer->Release();
	return ok;
}

// Frame timing for the log (ms, averaged over a couple of seconds of stereo frames): where the
// proxy's share of a frame goes.
static double g_timeWait, g_timeCopy, g_timePresent, g_timeHud, g_timeEnd;
static int g_timeFrames;
static DWORD g_timeStart;
static bool g_presentIsEye;  // the Present being hooked shows a stereo eye image on the monitor

static double NowMs()
{
	static LARGE_INTEGER freq;
	if (!freq.QuadPart)
		QueryPerformanceFrequency(&freq);
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return (double)t.QuadPart * 1000.0 / (double)freq.QuadPart;
}

static void LogTiming()
{
	if (!g_timeFrames++)
		g_timeStart = GetTickCount();
	if (GetTickCount() - g_timeStart < 2000)
		return;
	double n = g_timeFrames;
	Log("Proxy timing (ms, %d stereo frames): xrWaitFrame %.2f, eye copy %.2f, monitor present %.2f, HUD capture %.2f, xrEndFrame %.2f",
		g_timeFrames, g_timeWait / n, g_timeCopy / n, g_timePresent / n, g_timeHud / n, g_timeEnd / n);
	g_timeWait = g_timeCopy = g_timePresent = g_timeHud = g_timeEnd = 0.0;
	g_timeFrames = 0;
}

static void UpdateInput();

// Waits for and begins the next XR frame unless one is already begun, and reads the
// controllers for it. Returns false if no frame could be begun.
static bool BeginXrFrame()
{
	if (g_frameBegun)
		return true;
	if (!PollEvents() || !g_running)
		return false;

	XrFrameWaitInfo fwi = { XR_TYPE_FRAME_WAIT_INFO };
	g_frameState.type = XR_TYPE_FRAME_STATE;
	g_frameState.next = nullptr;
	double t0 = NowMs();
	XrResult r = x_xrWaitFrame(g_session, &fwi, &g_frameState);
	g_timeWait += NowMs() - t0;
	if (XR_FAILED(r))
	{
		Log("xrWaitFrame failed: %s", XrStr(r));
		return false;
	}
	XrFrameBeginInfo fbi = { XR_TYPE_FRAME_BEGIN_INFO };
	r = x_xrBeginFrame(g_session, &fbi);
	if (XR_FAILED(r))
	{
		Log("xrBeginFrame failed: %s", XrStr(r));
		return false;
	}
	g_frameBegun = true;
	g_viewsValid = g_stereoSubmitted = false;
	g_markerCount = 0;
	UpdateInput();

	if (g_recenterPendingTime && g_frameState.predictedDisplayTime >= g_recenterPendingTime)
	{
		g_recenterPendingTime = 0;
		++g_recenterCount;
		Log("Recenter #%d in effect", g_recenterCount);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Controller input
//
// One action set: "move" is the left thumbstick (trackpad on Vive wands) and "aim" is the right
// controller's aim pose. They're synced once per stereo frame in BeginStereoFrame and read by
// the game through GetInput.

static XrActionSet g_actionSet = XR_NULL_HANDLE;
static XrAction g_moveAction = XR_NULL_HANDLE;
static XrAction g_aimAction = XR_NULL_HANDLE;
static XrAction g_turnAction = XR_NULL_HANDLE;

// Controller types the bindings are suggested for, and their section in avp2xr.ini. On WMR and
// Vive controllers the grip is a click, and on Vive the trigger too.
static const struct ControllerProfile
{
	const char* path;
	const wchar_t* section;
	bool squeezeClick;
	bool triggerClick;
} g_profiles[] =
{
	{ "/interaction_profiles/oculus/touch_controller", L"Touch", false, false },
	{ "/interaction_profiles/valve/index_controller", L"Index", false, false },
	{ "/interaction_profiles/microsoft/motion_controller", L"WMR", true, false },
	{ "/interaction_profiles/htc/vive_controller", L"Vive", true, true },
	{ "/interaction_profiles/khr/simple_controller", L"Simple", false, false },
};
static const int g_numProfiles = sizeof(g_profiles) / sizeof(g_profiles[0]);

// Buttons: boolean actions, one per AVP2XR_BTN_* bit. Default bindings per controller type (in
// g_profiles order), written the way avp2xr.ini takes them (see ExpandBinding); nullptr = none.
static struct ButtonAction
{
	const char* name;
	const char* localized;
	unsigned int bit;
	const wchar_t* key;  // avp2xr.ini key
	const char* defaults[5];
	XrAction action;
} g_buttons[] =
{
	{ "fire", "Fire", AVP2XR_BTN_FIRE, L"Fire", { "right trigger", "right trigger", "right trigger", "right trigger", "right select" } },
	{ "altfire", "Alt fire", AVP2XR_BTN_ALTFIRE, L"AltFire", { "right grip", "right grip", "right grip", "right grip", nullptr } },
	{ "jump", "Jump", AVP2XR_BTN_JUMP, L"Jump", { "right a", "right a", nullptr, nullptr, nullptr } },
	{ "crouch", "Crouch", AVP2XR_BTN_CROUCH, L"Crouch", { "right b", "right b", nullptr, nullptr, nullptr } },
	{ "use", "Use", AVP2XR_BTN_USE, L"Use", { "left x", "left a", nullptr, nullptr, nullptr } },
	{ "vision", "Vision mode", AVP2XR_BTN_VISION, L"Vision", { "left y", "left b", nullptr, nullptr, nullptr } },
	{ "reload", "Reload", AVP2XR_BTN_RELOAD, L"Reload", { "left grip", "left grip", "left grip", "left grip", nullptr } },
	{ "taunt", "Taunt", AVP2XR_BTN_TAUNT, L"Taunt", { nullptr, nullptr, nullptr, nullptr, nullptr } },
	{ "menuselect", "Menu select", AVP2XR_BTN_MENUSELECT, L"MenuSelect", { "right a", "right a", "right trigger", "right trigger", "right select" } },
	{ "menuback", "Menu back", AVP2XR_BTN_MENUBACK, L"MenuBack", { "right b", "right b", "right menu", "right menu", "right menu" } },
	{ "pause", "Pause menu", AVP2XR_BTN_PAUSE, L"Pause", { "left menu", nullptr, "left menu", "left menu", nullptr } },
	{ "cloak", "Cloak (Predator)", AVP2XR_BTN_CLOAK, L"Cloak", { nullptr, nullptr, nullptr, nullptr, nullptr } },
	{ "flare", "Flare (Marine)", AVP2XR_BTN_FLARE, L"Flare", { nullptr, nullptr, nullptr, nullptr, nullptr } },
	{ "discretrieve", "Disc retrieve (Predator)", AVP2XR_BTN_DISCRETRIEVE, L"DiscRetrieve", { nullptr, nullptr, nullptr, nullptr, nullptr } },
	{ "wallwalk", "Wall-walk (Alien)", AVP2XR_BTN_WALLWALK, L"WallWalk", { nullptr, nullptr, nullptr, nullptr, nullptr } },
	{ "hack", "Hacking device / H item", AVP2XR_BTN_HACK, L"Hack", { nullptr, nullptr, nullptr, nullptr, nullptr } },
	{ "pounce", "Pounce (Alien)", AVP2XR_BTN_POUNCE, L"Pounce", { nullptr, nullptr, nullptr, nullptr, nullptr } },
	{ "objectives", "Mission objectives", AVP2XR_BTN_OBJECTIVES, L"Objectives", { nullptr, nullptr, nullptr, nullptr, nullptr } },
	{ "zoom", "Zoom (Predator)", AVP2XR_BTN_ZOOM, L"Zoom", { nullptr, nullptr, nullptr, nullptr, nullptr } },
	{ "shoulderlamp", "Shoulder lamp (Marine)", AVP2XR_BTN_SHOULDERLAMP, L"ShoulderLamp", { nullptr, nullptr, nullptr, nullptr, nullptr } },
	{ "torchsift", "Welding torch / energy sift", AVP2XR_BTN_TORCHSIFT, L"TorchSift", { nullptr, nullptr, nullptr, nullptr, nullptr } },
	{ "medicomp", "Medicomp (Predator)", AVP2XR_BTN_MEDICOMP, L"Medicomp", { nullptr, nullptr, nullptr, nullptr, nullptr } },
	{ "nextweapon", "Next weapon", AVP2XR_BTN_NEXTWEAPON, L"NextWeapon", { "right thumbstick up", "right thumbstick up", "right thumbstick up", "right trackpad up", nullptr } },
	{ "prevweapon", "Previous weapon", AVP2XR_BTN_PREVWEAPON, L"PrevWeapon", { "right thumbstick down", "right thumbstick down", "right thumbstick down", "right trackpad down", nullptr } },
	{ "adjust", "Hand adjust mode", AVP2XR_BTN_ADJUST, L"Adjust", { nullptr, nullptr, nullptr, nullptr, nullptr } },
};
// The sticks (vector actions): movement and snap turn / weapon cycling
static const char* const g_moveDefaults[5] = { "left thumbstick", "left thumbstick", "left thumbstick", "left trackpad", nullptr };
static const char* const g_turnDefaults[5] = { "right thumbstick", "right thumbstick", "right thumbstick", "right trackpad", nullptr };
static const size_t g_numButtons = sizeof(g_buttons) / sizeof(g_buttons[0]);
static XrSpace g_aimSpace = XR_NULL_HANDLE;
static XrAction g_offAction = XR_NULL_HANDLE;   // left controller grip pose (two-handed aiming)
static XrSpace g_offSpace = XR_NULL_HANDLE;
static XrAction g_gripAction = XR_NULL_HANDLE;  // right controller grip pose (where the palm is)
static XrSpace g_gripSpace = XR_NULL_HANDLE;
static Avp2XrInput g_input;
static bool g_inputReady;

static XrPath Path(const char* s)
{
	XrPath p = XR_NULL_PATH;
	x_xrStringToPath(g_instance, s, &p);
	return p;
}

// Turns one avp2xr.ini binding into an OpenXR path: either a full path ("/user/hand/...") or
// "<left|right> <input>", e.g. "right trigger", "left x", "right grip", "left thumbstick". For
// buttons the input's click (value for trigger and grip where the controller has one, touch for
// thumbrest) is used; for sticks the 2D input itself. Returns false if it can't be read.
static bool ExpandBinding(const char* text, const ControllerProfile& profile, bool stick, char* out, size_t outSize)
{
	while (*text == ' ')
		++text;
	if (*text == '/')
	{
		strcpy_s(out, outSize, text);
		size_t n = strlen(out);
		while (n && out[n - 1] == ' ')
			out[--n] = 0;
		return n > 0;
	}

	char hand[16] = "", input[32] = "";
	if (sscanf_s(text, "%15s %31s", hand, (unsigned)sizeof(hand), input, (unsigned)sizeof(input)) != 2)
		return false;
	_strlwr_s(hand);
	_strlwr_s(input);
	if (strcmp(hand, "left") && strcmp(hand, "right"))
		return false;
	if (!strcmp(input, "grip"))
		strcpy_s(input, "squeeze");

	const char* component = "click";
	if (stick)
		component = nullptr;
	else if (!strcmp(input, "trigger"))
		component = profile.triggerClick ? "click" : "value";
	else if (!strcmp(input, "squeeze"))
		component = profile.squeezeClick ? "click" : "value";
	else if (!strcmp(input, "thumbrest"))
		component = "touch";
	if (component)
		sprintf_s(out, outSize, "/user/hand/%s/input/%s/%s", hand, input, component);
	else
		sprintf_s(out, outSize, "/user/hand/%s/input/%s", hand, input);
	return true;
}

// The bindings text for one action: avp2xr.ini's [section] key if it's there (empty = unbound),
// otherwise the default. Sets *custom if it differs from the default.
static void BindingText(const wchar_t* ini, const ControllerProfile& profile, const wchar_t* key, const char* def,
	char* out, size_t outSize, bool* custom)
{
	wchar_t buf[256];
	GetPrivateProfileStringW(profile.section, key, L"\x01", buf, 256, ini);
	if (buf[0] == 1)
	{
		strcpy_s(out, outSize, def ? def : "");
		return;
	}
	WideCharToMultiByte(CP_UTF8, 0, buf, -1, out, (int)outSize, nullptr, nullptr);
	if (strcmp(out, def ? def : ""))
		*custom = true;
}

// Long presses: an input written "long <input>" in avp2xr.ini gives its action only after it's
// held for LongPressTime; released sooner, it gives the input's other (short) actions instead, as
// a brief press on release. Such inputs are read through the raw actions below, not bound to the
// game's actions directly. Per controller type, since each has its own section.
static const int kMaxRawInputs = 8;
static XrAction g_rawActions[kMaxRawInputs];
static struct RawInput { unsigned int shortBits, longBits; } g_rawInputs[5][kMaxRawInputs];
static int g_numRawInputs[5];
static int g_activeProfile = -1;  // g_profiles index of the controllers in use, -1 = not known yet
static struct RawState { bool down, longOn; DWORD start; int pulse; } g_rawState[kMaxRawInputs];

// Combos: a button written "<input> + <input>" (e.g. Adjust=left thumbstick + right b) is pressed
// by holding both. Each of the two is read through an action of its own as well as being bound as
// usual. Once both are down, the other actions on those two inputs are held back until both are let
// go, so pressing the second doesn't also do its own thing (the first does, until the second is
// pressed). Per controller type, like the long presses.
static const int kMaxCombos = 4;
static XrAction g_comboActions[kMaxCombos][2];
static struct Combo { unsigned int bits, otherBits; char path[2][128]; } g_combos[5][kMaxCombos];
static int g_numCombos[5];
static bool g_comboOn[kMaxCombos];

// Stick directions: a button written "<left|right> <thumbstick|trackpad> <up|down|left|right>" is
// pressed by pushing that stick that way (past 0.6, and more that way than across; released below
// 0.3). OpenXR has no such inputs, so the sticks are read through the actions below and the
// directions worked out here. Per controller type, like the long presses.
static const char* const g_dirStickInputs[4] = { "left thumbstick", "right thumbstick", "left trackpad", "right trackpad" };
static XrAction g_dirStickActions[4];
static const int kMaxStickDirs = 16;
static struct StickDir { unsigned int bits; int stick; int dir; } g_stickDirs[5][kMaxStickDirs];  // dir: 0 up 1 down 2 left 3 right
static int g_numStickDirs[5];
static bool g_stickDirOn[kMaxStickDirs];

// Parses a stick direction binding; returns false if it isn't one
static bool ParseStickDir(const char* text, int* stick, int* dir)
{
	char hand[16] = "", input[32] = "", way[16] = "", extra[8] = "";
	if (sscanf_s(text, "%15s %31s %15s %7s", hand, (unsigned)sizeof(hand), input, (unsigned)sizeof(input), way, (unsigned)sizeof(way),
		extra, (unsigned)sizeof(extra)) != 3)
		return false;
	static const char* const ways[4] = { "up", "down", "left", "right" };
	*dir = -1;
	for (int d = 0; d < 4; ++d)
		if (!_stricmp(way, ways[d]))
			*dir = d;
	if (*dir < 0)
		return false;
	char name[64];
	sprintf_s(name, "%s %s", hand, input);
	for (int k = 0; k < 4; ++k)
		if (!_stricmp(name, g_dirStickInputs[k]))
		{
			*stick = k;
			return true;
		}
	return false;
}

// Suggests the bindings for one controller type, from avp2xr.ini where it has them. If the runtime
// refuses a customised set, the defaults are used instead so the controllers still work.
static void SuggestBindings(const wchar_t* ini, int nProfile)
{
	const ControllerProfile& profile = g_profiles[nProfile];
	const int maxBindings = 128;
	XrActionSuggestedBinding b[maxBindings];

	for (int attempt = 0; attempt < 2; ++attempt)
	{
		const bool useIni = attempt == 0;
		bool custom = false, ok = true;
		uint32_t n = 0;
		char summary[8192] = "";

		b[n++] = { g_aimAction, Path("/user/hand/right/input/aim/pose") };
		b[n++] = { g_offAction, Path("/user/hand/left/input/grip/pose") };
		b[n++] = { g_gripAction, Path("/user/hand/right/input/grip/pose") };

		// Every action: the two sticks and all the buttons, with its bindings text
		struct { XrAction action; const wchar_t* key; const char* def; bool stick; unsigned int bit; char text[256]; }
			all[2 + sizeof(g_buttons) / sizeof(g_buttons[0])];
		int count = 0;
		all[count++] = { g_moveAction, L"Move", g_moveDefaults[nProfile], true, 0 };
		all[count++] = { g_turnAction, L"Turn", g_turnDefaults[nProfile], true, 0 };
		for (size_t i = 0; i < g_numButtons; ++i)
			all[count++] = { g_buttons[i].action, g_buttons[i].key, g_buttons[i].defaults[nProfile], false, g_buttons[i].bit };
		for (int i = 0; i < count; ++i)
		{
			if (useIni)
				BindingText(ini, profile, all[i].key, all[i].def, all[i].text, sizeof(all[i].text), &custom);
			else
				strcpy_s(all[i].text, all[i].def ? all[i].def : "");
			if (all[i].bit == AVP2XR_BTN_ADJUST && !g_cfg.adjustHandsLive)
				all[i].text[0] = 0;  // AdjustHandsLive=no: hand adjust mode isn't bound
			char part[320];
			sprintf_s(part, "%s%S=%s", i ? ", " : "", all[i].key, all[i].text);
			strcat_s(summary, part);
		}

		// Two passes over every binding: first find the inputs used for long presses, then bind
		char shared[kMaxRawInputs][128];
		int numShared = 0;
		RawInput raw[kMaxRawInputs];
		memset(raw, 0, sizeof(raw));
		StickDir dirs[kMaxStickDirs];
		int numDirs = 0;
		bool stickUsed[4] = { false, false, false, false };
		Combo combos[kMaxCombos];
		int numCombos = 0;
		memset(combos, 0, sizeof(combos));
		// The inputs bound straight to buttons, to find what a combo's inputs also do
		struct { char path[128]; unsigned int bit; } direct[maxBindings];
		int numDirect = 0;
		for (int pass = 0; pass < 2 && ok; ++pass)
		{
			for (int i = 0; i < count && ok; ++i)
			{
				char list[256];
				strcpy_s(list, all[i].text);
				char* next = nullptr;
				for (char* item = strtok_s(list, ",", &next); item && ok; item = strtok_s(nullptr, ",", &next))
				{
					while (*item == ' ')
						++item;
					if (!*item)
						continue;
					bool isLong = !_strnicmp(item, "long ", 5);
					if (isLong)
						item += 5;
					char* plus = strchr(item, '+');
					if (plus)
					{
						*plus = 0;
						char* second = plus + 1;
						while (*second == ' ')
							++second;
						for (char* end = item + strlen(item); end > item && end[-1] == ' '; )
							*--end = 0;
						char paths[2][128];
						if (isLong || all[i].stick || numCombos == kMaxCombos ||
							!ExpandBinding(item, profile, false, paths[0], sizeof(paths[0])) ||
							!ExpandBinding(second, profile, false, paths[1], sizeof(paths[1])))
						{
							Log("Input: [%S] can't use the combo \"%s%s + %s\"%s", profile.section, isLong ? "long " : "", item, second,
								numCombos == kMaxCombos ? " (too many)" : "");
							ok = false;
							break;
						}
						if (pass == 1)
						{
							Combo& c = combos[numCombos++];
							c.bits = all[i].bit;
							strcpy_s(c.path[0], paths[0]);
							strcpy_s(c.path[1], paths[1]);
						}
						continue;
					}
					int dirStick, dir;
					if (!all[i].stick && ParseStickDir(item, &dirStick, &dir))
					{
						if (isLong || numDirs == kMaxStickDirs)
						{
							Log("Input: [%S] can't use the stick direction \"%s%s\"%s", profile.section, isLong ? "long " : "", item,
								isLong ? " as a long press" : " (too many)");
							ok = false;
							break;
						}
						if (pass == 1)
						{
							dirs[numDirs++] = { all[i].bit, dirStick, dir };
							stickUsed[dirStick] = true;
						}
						continue;
					}
					char path[128];
					if (!ExpandBinding(item, profile, all[i].stick, path, sizeof(path)) || (isLong && all[i].stick))
					{
						Log("Input: [%S] can't read the binding \"%s%s\"", profile.section, isLong ? "long " : "", item);
						ok = false;
						break;
					}
					int k = 0;
					while (k < numShared && strcmp(shared[k], path))
						++k;
					if (pass == 0)
					{
						if (isLong && k == numShared)
						{
							if (numShared == kMaxRawInputs)
							{
								Log("Input: [%S] too many long-press inputs (at most %d)", profile.section, kMaxRawInputs);
								ok = false;
								break;
							}
							strcpy_s(shared[numShared++], path);
						}
						continue;
					}
					if (k < numShared && !all[i].stick)
					{
						if (isLong)
							raw[k].longBits |= all[i].bit;
						else
							raw[k].shortBits |= all[i].bit;
						continue;
					}
					XrPath xp = Path(path);
					if (xp == XR_NULL_PATH || n >= (uint32_t)maxBindings)
					{
						Log("Input: [%S] bad binding path %s", profile.section, path);
						ok = false;
						break;
					}
					b[n++] = { all[i].action, xp };
					if (!all[i].stick && numDirect < maxBindings)
					{
						strcpy_s(direct[numDirect].path, path);
						direct[numDirect++].bit = all[i].bit;
					}
				}
			}
		}

		// Each combo's inputs: read through its own actions, and what else they do (held back while
		// the combo is on), bound straight or as long presses
		for (int c = 0; c < numCombos && ok; ++c)
		{
			for (int h = 0; h < 2 && ok; ++h)
			{
				for (int d = 0; d < numDirect; ++d)
					if (!strcmp(direct[d].path, combos[c].path[h]))
						combos[c].otherBits |= direct[d].bit;
				for (int k = 0; k < numShared; ++k)
					if (!strcmp(shared[k], combos[c].path[h]))
						combos[c].otherBits |= raw[k].shortBits | raw[k].longBits;
				XrPath xp = Path(combos[c].path[h]);
				if (xp == XR_NULL_PATH || n >= (uint32_t)maxBindings)
					ok = false;
				else
					b[n++] = { g_comboActions[c][h], xp };
			}
			combos[c].otherBits &= ~combos[c].bits;
		}
		for (int k = 0; k < numShared && ok; ++k)
		{
			XrPath xp = Path(shared[k]);
			if (xp == XR_NULL_PATH || n >= (uint32_t)maxBindings)
				ok = false;
			else
				b[n++] = { g_rawActions[k], xp };
		}
		for (int k = 0; k < 4 && ok; ++k)
		{
			if (!stickUsed[k])
				continue;
			char path[128];
			XrPath xp = XR_NULL_PATH;
			if (ExpandBinding(g_dirStickInputs[k], profile, true, path, sizeof(path)))
				xp = Path(path);
			if (xp == XR_NULL_PATH || n >= (uint32_t)maxBindings)
				ok = false;
			else
				b[n++] = { g_dirStickActions[k], xp };
		}

		if (ok)
		{
			XrInteractionProfileSuggestedBinding sb = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
			sb.interactionProfile = Path(profile.path);
			sb.countSuggestedBindings = n;
			sb.suggestedBindings = b;
			XrResult r = x_xrSuggestInteractionProfileBindings(g_instance, &sb);
			if (XR_SUCCEEDED(r))
			{
				g_numRawInputs[nProfile] = numShared;
				memcpy(g_rawInputs[nProfile], raw, sizeof(raw));
				g_numStickDirs[nProfile] = numDirs;
				memcpy(g_stickDirs[nProfile], dirs, sizeof(dirs));
				g_numCombos[nProfile] = numCombos;
				memcpy(g_combos[nProfile], combos, sizeof(combos));
				if (custom)
					Log("Input: [%S] bindings from avp2xr.ini: %s", profile.section, summary);
				return;
			}
			Log("Input: the runtime refused the [%S] bindings (%s): %s", profile.section, XrStr(r), summary);
		}
		g_numRawInputs[nProfile] = 0;
		g_numStickDirs[nProfile] = 0;
		g_numCombos[nProfile] = 0;
		if (!useIni || !custom)
			return;  // the defaults failed too (or there was nothing custom to fall back from)
		Log("Input: using the default [%S] bindings instead", profile.section);
	}
}

// Which controller type is in use (for its long-press inputs): asked when the runtime says it changed
static void UpdateActiveProfile()
{
	const char* hands[2] = { "/user/hand/left", "/user/hand/right" };
	for (int h = 0; h < 2; ++h)
	{
		XrInteractionProfileState st = { XR_TYPE_INTERACTION_PROFILE_STATE };
		if (XR_FAILED(x_xrGetCurrentInteractionProfile(g_session, Path(hands[h]), &st)) || st.interactionProfile == XR_NULL_PATH)
			continue;
		for (int i = 0; i < g_numProfiles; ++i)
		{
			if (st.interactionProfile == Path(g_profiles[i].path))
			{
				if (g_activeProfile != i)
					Log("Input: controllers are [%S] (%d long-press inputs)", g_profiles[i].section, g_numRawInputs[i]);
				g_activeProfile = i;
				memset(g_rawState, 0, sizeof(g_rawState));
				memset(g_stickDirOn, 0, sizeof(g_stickDirOn));
				memset(g_comboOn, 0, sizeof(g_comboOn));
				return;
			}
		}
	}
}

// Must run after the session is created and before the first xrSyncActions. Failure only
// means no controller input; stereo still works.
static void InitInput()
{
	g_inputReady = false;
	memset(&g_input, 0, sizeof(g_input));

	XrActionSetCreateInfo asci = { XR_TYPE_ACTION_SET_CREATE_INFO };
	strcpy_s(asci.actionSetName, "gameplay");
	strcpy_s(asci.localizedActionSetName, "Gameplay");
	XrResult r = x_xrCreateActionSet(g_instance, &asci, &g_actionSet);
	if (XR_FAILED(r)) { Log("Input: xrCreateActionSet failed: %s", XrStr(r)); return; }

	XrActionCreateInfo aci = { XR_TYPE_ACTION_CREATE_INFO };
	aci.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
	strcpy_s(aci.actionName, "move");
	strcpy_s(aci.localizedActionName, "Move");
	r = x_xrCreateAction(g_actionSet, &aci, &g_moveAction);
	if (XR_FAILED(r)) { Log("Input: creating the move action failed: %s", XrStr(r)); return; }

	aci.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
	strcpy_s(aci.actionName, "turn");
	strcpy_s(aci.localizedActionName, "Snap turn");
	r = x_xrCreateAction(g_actionSet, &aci, &g_turnAction);
	if (XR_FAILED(r)) { Log("Input: creating the turn action failed: %s", XrStr(r)); return; }

	aci.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
	for (size_t i = 0; i < g_numButtons; ++i)
	{
		strcpy_s(aci.actionName, g_buttons[i].name);
		strcpy_s(aci.localizedActionName, g_buttons[i].localized);
		r = x_xrCreateAction(g_actionSet, &aci, &g_buttons[i].action);
		if (XR_FAILED(r)) { Log("Input: creating the %s action failed: %s", g_buttons[i].name, XrStr(r)); return; }
	}

	aci.actionType = XR_ACTION_TYPE_POSE_INPUT;
	strcpy_s(aci.actionName, "aim");
	strcpy_s(aci.localizedActionName, "Aim");
	r = x_xrCreateAction(g_actionSet, &aci, &g_aimAction);
	if (XR_FAILED(r)) { Log("Input: creating the aim action failed: %s", XrStr(r)); return; }

	strcpy_s(aci.actionName, "offhand");
	strcpy_s(aci.localizedActionName, "Off hand");
	r = x_xrCreateAction(g_actionSet, &aci, &g_offAction);
	if (XR_FAILED(r)) { Log("Input: creating the off-hand action failed: %s", XrStr(r)); return; }

	strcpy_s(aci.actionName, "grip");
	strcpy_s(aci.localizedActionName, "Weapon hand");
	r = x_xrCreateAction(g_actionSet, &aci, &g_gripAction);
	if (XR_FAILED(r)) { Log("Input: creating the grip action failed: %s", XrStr(r)); return; }

	aci.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
	for (int k = 0; k < kMaxRawInputs; ++k)
	{
		sprintf_s(aci.actionName, "longpress%d", k);
		sprintf_s(aci.localizedActionName, "Long-press input %d", k + 1);
		r = x_xrCreateAction(g_actionSet, &aci, &g_rawActions[k]);
		if (XR_FAILED(r)) { Log("Input: creating the long-press actions failed: %s", XrStr(r)); return; }
	}

	for (int c = 0; c < kMaxCombos; ++c)
	{
		for (int h = 0; h < 2; ++h)
		{
			sprintf_s(aci.actionName, "combo%d_%d", c, h);
			sprintf_s(aci.localizedActionName, "Combo %d input %d", c + 1, h + 1);
			r = x_xrCreateAction(g_actionSet, &aci, &g_comboActions[c][h]);
			if (XR_FAILED(r)) { Log("Input: creating the combo actions failed: %s", XrStr(r)); return; }
		}
	}

	aci.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
	static const char* const dirNames[4] = { "leftstickdirs", "rightstickdirs", "lefttrackpaddirs", "righttrackpaddirs" };
	static const char* const dirLocalized[4] = { "Left stick directions", "Right stick directions", "Left trackpad directions", "Right trackpad directions" };
	for (int k = 0; k < 4; ++k)
	{
		strcpy_s(aci.actionName, dirNames[k]);
		strcpy_s(aci.localizedActionName, dirLocalized[k]);
		r = x_xrCreateAction(g_actionSet, &aci, &g_dirStickActions[k]);
		if (XR_FAILED(r)) { Log("Input: creating the stick direction actions failed: %s", XrStr(r)); return; }
	}

	wchar_t ini[MAX_PATH];
	swprintf_s(ini, L"%savp2xr.ini", g_moduleDir);
	for (int i = 0; i < g_numProfiles; ++i)
		SuggestBindings(ini, i);

	XrSessionActionSetsAttachInfo ai = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
	ai.countActionSets = 1;
	ai.actionSets = &g_actionSet;
	r = x_xrAttachSessionActionSets(g_session, &ai);
	if (XR_FAILED(r)) { Log("Input: xrAttachSessionActionSets failed: %s", XrStr(r)); return; }

	XrActionSpaceCreateInfo sci = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
	sci.action = g_aimAction;
	sci.poseInActionSpace.orientation.w = 1.0f;
	r = x_xrCreateActionSpace(g_session, &sci, &g_aimSpace);
	if (XR_FAILED(r)) { Log("Input: creating the aim space failed: %s", XrStr(r)); return; }

	sci.action = g_offAction;
	r = x_xrCreateActionSpace(g_session, &sci, &g_offSpace);
	if (XR_FAILED(r)) { Log("Input: creating the off-hand space failed: %s", XrStr(r)); return; }

	sci.action = g_gripAction;
	r = x_xrCreateActionSpace(g_session, &sci, &g_gripSpace);
	if (XR_FAILED(r)) { Log("Input: creating the grip space failed: %s", XrStr(r)); return; }

	g_inputReady = true;
	Log("Input: left stick moves, right controller aims, right stick turns, buttons bound");
}

// Reads the controllers for the begun frame (poses predicted for its display time).
static void UpdateInput()
{
	if (!g_inputReady)
		return;

	XrActiveActionSet active = { g_actionSet, XR_NULL_PATH };
	XrActionsSyncInfo si = { XR_TYPE_ACTIONS_SYNC_INFO };
	si.countActiveActionSets = 1;
	si.activeActionSets = &active;
	XrResult r = x_xrSyncActions(g_session, &si);
	if (r != XR_SUCCESS)  // XR_SESSION_NOT_FOCUSED means another app has input
	{
		g_input.aimValid = g_input.moveValid = g_input.turnValid = g_input.offValid = g_input.gripValid = 0;
		g_input.buttons = 0;
		return;
	}

	XrActionStateGetInfo gi = { XR_TYPE_ACTION_STATE_GET_INFO };
	gi.action = g_moveAction;
	XrActionStateVector2f move = { XR_TYPE_ACTION_STATE_VECTOR2F };
	if (XR_SUCCEEDED(x_xrGetActionStateVector2f(g_session, &gi, &move)) && move.isActive)
	{
		g_input.moveValid = 1;
		g_input.move[0] = move.currentState.x;
		g_input.move[1] = move.currentState.y;
	}
	else
		g_input.moveValid = 0;

	gi.action = g_turnAction;
	XrActionStateVector2f turn = { XR_TYPE_ACTION_STATE_VECTOR2F };
	if (XR_SUCCEEDED(x_xrGetActionStateVector2f(g_session, &gi, &turn)) && turn.isActive)
	{
		g_input.turnValid = 1;
		g_input.turn = turn.currentState.x;
		g_input.turnY = turn.currentState.y;
	}
	else
		g_input.turnValid = 0;

	g_input.buttons = 0;
	for (size_t i = 0; i < g_numButtons; ++i)
	{
		gi.action = g_buttons[i].action;
		XrActionStateBoolean bs = { XR_TYPE_ACTION_STATE_BOOLEAN };
		if (XR_SUCCEEDED(x_xrGetActionStateBoolean(g_session, &gi, &bs)) && bs.isActive && bs.currentState)
			g_input.buttons |= g_buttons[i].bit;
	}

	// Long-press inputs: held past LongPressTime they give their long actions until released;
	// let go sooner, their short actions for a moment
	if (g_activeProfile >= 0)
	{
		DWORD now = GetTickCount();
		DWORD hold = (DWORD)(g_cfg.longPressTime * 1000.0f);
		for (int k = 0; k < g_numRawInputs[g_activeProfile]; ++k)
		{
			gi.action = g_rawActions[k];
			XrActionStateBoolean bs = { XR_TYPE_ACTION_STATE_BOOLEAN };
			bool down = XR_SUCCEEDED(x_xrGetActionStateBoolean(g_session, &gi, &bs)) && bs.isActive && bs.currentState;
			RawState& st = g_rawState[k];
			const RawInput& in = g_rawInputs[g_activeProfile][k];
			if (down && !st.down)
			{
				st.start = now;
				st.longOn = false;
			}
			if (down && !st.longOn && now - st.start >= hold)
				st.longOn = true;
			if (!down && st.down && !st.longOn)
				st.pulse = 2;  // a couple of frames, so the game sees the press and the release
			st.down = down;
			if (down && st.longOn)
				g_input.buttons |= in.longBits;
			if (st.pulse > 0)
			{
				g_input.buttons |= in.shortBits;
				--st.pulse;
			}
		}
	}

	// Stick directions
	if (g_activeProfile >= 0 && g_numStickDirs[g_activeProfile] > 0)
	{
		XrVector2f v[4] = {};
		bool valid[4] = { false, false, false, false };
		for (int k = 0; k < 4; ++k)
		{
			gi.action = g_dirStickActions[k];
			XrActionStateVector2f vs = { XR_TYPE_ACTION_STATE_VECTOR2F };
			if (XR_SUCCEEDED(x_xrGetActionStateVector2f(g_session, &gi, &vs)) && vs.isActive)
			{
				v[k] = vs.currentState;
				valid[k] = true;
			}
		}
		for (int j = 0; j < g_numStickDirs[g_activeProfile]; ++j)
		{
			const StickDir& sd = g_stickDirs[g_activeProfile][j];
			float x = valid[sd.stick] ? v[sd.stick].x : 0.0f, y = valid[sd.stick] ? v[sd.stick].y : 0.0f;
			float along = sd.dir == 0 ? y : sd.dir == 1 ? -y : sd.dir == 2 ? -x : x;
			float across = sd.dir < 2 ? fabsf(x) : fabsf(y);
			if (g_stickDirOn[j])
				g_stickDirOn[j] = along > 0.3f;
			else
				g_stickDirOn[j] = along > 0.6f && along > across;
			if (g_stickDirOn[j])
				g_input.buttons |= sd.bits;
		}
	}

	// Combos: on with both inputs down; their inputs' other actions are held back until both are up
	if (g_activeProfile >= 0)
	{
		for (int c = 0; c < g_numCombos[g_activeProfile]; ++c)
		{
			const Combo& combo = g_combos[g_activeProfile][c];
			bool down[2];
			for (int h = 0; h < 2; ++h)
			{
				gi.action = g_comboActions[c][h];
				XrActionStateBoolean bs = { XR_TYPE_ACTION_STATE_BOOLEAN };
				down[h] = XR_SUCCEEDED(x_xrGetActionStateBoolean(g_session, &gi, &bs)) && bs.isActive && bs.currentState;
			}
			if (down[0] && down[1])
				g_comboOn[c] = true;
			else if (!down[0] && !down[1])
				g_comboOn[c] = false;
			if (g_comboOn[c])
			{
				g_input.buttons &= ~combo.otherBits;
				if (down[0] && down[1])
					g_input.buttons |= combo.bits;
			}
		}
	}

	XrSpaceLocation loc = { XR_TYPE_SPACE_LOCATION };
	const XrSpaceLocationFlags needed = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
	if (XR_SUCCEEDED(x_xrLocateSpace(g_aimSpace, g_space, g_frameState.predictedDisplayTime, &loc)) &&
		(loc.locationFlags & needed) == needed)
	{
		g_input.aimValid = 1;
		g_input.aimOrientation[0] = loc.pose.orientation.x;
		g_input.aimOrientation[1] = loc.pose.orientation.y;
		g_input.aimOrientation[2] = loc.pose.orientation.z;
		g_input.aimOrientation[3] = loc.pose.orientation.w;
		g_input.aimPosition[0] = loc.pose.position.x;
		g_input.aimPosition[1] = loc.pose.position.y;
		g_input.aimPosition[2] = loc.pose.position.z;
	}
	else
		g_input.aimValid = 0;

	XrSpaceLocation off = { XR_TYPE_SPACE_LOCATION };
	if (XR_SUCCEEDED(x_xrLocateSpace(g_offSpace, g_space, g_frameState.predictedDisplayTime, &off)) &&
		(off.locationFlags & needed) == needed)
	{
		g_input.offValid = 1;
		g_input.offOrientation[0] = off.pose.orientation.x;
		g_input.offOrientation[1] = off.pose.orientation.y;
		g_input.offOrientation[2] = off.pose.orientation.z;
		g_input.offOrientation[3] = off.pose.orientation.w;
		g_input.offPosition[0] = off.pose.position.x;
		g_input.offPosition[1] = off.pose.position.y;
		g_input.offPosition[2] = off.pose.position.z;
	}
	else
		g_input.offValid = 0;

	XrSpaceLocation grip = { XR_TYPE_SPACE_LOCATION };
	if (XR_SUCCEEDED(x_xrLocateSpace(g_gripSpace, g_space, g_frameState.predictedDisplayTime, &grip)) &&
		(grip.locationFlags & needed) == needed)
	{
		g_input.gripValid = 1;
		g_input.gripOrientation[0] = grip.pose.orientation.x;
		g_input.gripOrientation[1] = grip.pose.orientation.y;
		g_input.gripOrientation[2] = grip.pose.orientation.z;
		g_input.gripOrientation[3] = grip.pose.orientation.w;
		g_input.gripPosition[0] = grip.pose.position.x;
		g_input.gripPosition[1] = grip.pose.position.y;
		g_input.gripPosition[2] = grip.pose.position.z;
	}
	else
		g_input.gripValid = 0;

	static int lastAim = -1, lastMove = -1;
	if (g_input.aimValid != lastAim || g_input.moveValid != lastMove)
	{
		lastAim = g_input.aimValid;
		lastMove = g_input.moveValid;
		Log("Input: aim controller %s, move stick %s", lastAim ? "tracked" : "not tracked", lastMove ? "active" : "inactive");
	}
}

// The action set, actions and aim space are destroyed along with the instance and session.
static void ResetInput()
{
	g_actionSet = XR_NULL_HANDLE;
	g_moveAction = g_aimAction = g_turnAction = XR_NULL_HANDLE;
	for (size_t i = 0; i < g_numButtons; ++i)
		g_buttons[i].action = XR_NULL_HANDLE;
	g_aimSpace = XR_NULL_HANDLE;
	g_offAction = XR_NULL_HANDLE;
	g_offSpace = XR_NULL_HANDLE;
	g_gripAction = XR_NULL_HANDLE;
	g_gripSpace = XR_NULL_HANDLE;
	for (int k = 0; k < kMaxRawInputs; ++k)
		g_rawActions[k] = XR_NULL_HANDLE;
	for (int k = 0; k < 4; ++k)
		g_dirStickActions[k] = XR_NULL_HANDLE;
	memset(g_stickDirOn, 0, sizeof(g_stickDirOn));
	for (int c = 0; c < kMaxCombos; ++c)
		g_comboActions[c][0] = g_comboActions[c][1] = XR_NULL_HANDLE;
	memset(g_comboOn, 0, sizeof(g_comboOn));
	g_activeProfile = -1;
	memset(g_rawState, 0, sizeof(g_rawState));
	g_inputReady = false;
	memset(&g_input, 0, sizeof(g_input));
}

static int __cdecl Api_GetInput(Avp2XrInput* input)
{
	if (!input)
		return 0;
	if (g_xrState != XRS_ACTIVE || !g_inputReady)
		memset(input, 0, sizeof(*input));
	else
		*input = g_input;
	input->recenterCount = g_recenterCount;
	return input->aimValid || input->moveValid || input->turnValid || input->buttons;
}

static int __cdecl Api_BeginStereoFrame(Avp2XrView views[2])
{
	if (g_xrState != XRS_ACTIVE || !BeginXrFrame() || !g_frameState.shouldRender)
		return 0;

	if (!g_viewsValid)
	{
		XrViewLocateInfo li = { XR_TYPE_VIEW_LOCATE_INFO };
		li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		li.displayTime = g_frameState.predictedDisplayTime;
		li.space = g_space;
		XrViewState vs = { XR_TYPE_VIEW_STATE };
		uint32_t count = 0;
		g_views[0] = g_views[1] = { XR_TYPE_VIEW };
		XrResult r = x_xrLocateViews(g_session, &li, &vs, 2, &count, g_views);
		if (XR_FAILED(r))
		{
			Log("xrLocateViews failed: %s", XrStr(r));
			return 0;
		}
		const XrViewStateFlags needed = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
		if (count != 2 || (vs.viewStateFlags & needed) != needed)
			return 0;  // not tracking (e.g. headset off the face); fall back to the flat screen
		g_viewsValid = true;
	}

	for (int eye = 0; eye < 2; ++eye)
	{
		const XrView& v = g_views[eye];
		views[eye].orientation[0] = v.pose.orientation.x;
		views[eye].orientation[1] = v.pose.orientation.y;
		views[eye].orientation[2] = v.pose.orientation.z;
		views[eye].orientation[3] = v.pose.orientation.w;
		views[eye].position[0] = v.pose.position.x;
		views[eye].position[1] = v.pose.position.y;
		views[eye].position[2] = v.pose.position.z;
		views[eye].fov[0] = v.fov.angleLeft;
		views[eye].fov[1] = v.fov.angleRight;
		views[eye].fov[2] = v.fov.angleUp;
		views[eye].fov[3] = v.fov.angleDown;
	}
	return 1;
}

static void __cdecl Api_SubmitStereo(const Avp2XrEyeSubmit eyes[2], int hudFlags)
{
	if (!g_frameBegun || !g_viewsValid)
		return;
	g_eyeSubmit[0] = eyes[0];
	g_eyeSubmit[1] = eyes[1];
	g_stereoSubmitted = true;
	g_hudFollows = (hudFlags & AVP2XR_HUD_FOLLOWS) != 0;
}

static int g_gameWidth, g_gameHeight;  // the game's own screen size; 0 until reported

static void __cdecl Api_SetGameResolution(int width, int height)
{
	if (width <= 0 || height <= 0 || (width == g_gameWidth && height == g_gameHeight))
		return;
	g_gameWidth = width;
	g_gameHeight = height;
	Log("Game resolution %dx%d", width, height);
}

static void __cdecl Api_SetCrosshairImage(int width, int height, const unsigned int* pixels)
{
	unsigned int* copy = nullptr;
	if (pixels && width > 0 && height > 0 && width <= 1024 && height <= 1024)
	{
		copy = (unsigned int*)malloc((size_t)width * height * 4);
		if (!copy)
			return;
		memcpy(copy, pixels, (size_t)width * height * 4);
	}
	free(g_crosshairPixels);
	g_crosshairPixels = copy;
	g_crosshairWidth = copy ? width : 0;
	g_crosshairHeight = copy ? height : 0;
	g_crosshairImageNew = true;
}

static void __cdecl Api_SubmitCrosshairs(const Avp2XrCrosshair crosshairs[2])
{
	if (!g_frameBegun || !g_viewsValid || !crosshairs)
		return;
	g_crosshairs[0] = crosshairs[0];
	g_crosshairs[1] = crosshairs[1];
}

static void __cdecl Api_SetMarkerImage(int image, int width, int height, const unsigned int* pixels)
{
	if (image < 0 || image >= AVP2XR_MAX_MARKER_IMAGES)
		return;
	unsigned int* copy = nullptr;
	if (pixels && width > 0 && height > 0 && width <= 1024 && height <= 1024)
	{
		copy = (unsigned int*)malloc((size_t)width * height * 4);
		if (!copy)
			return;
		memcpy(copy, pixels, (size_t)width * height * 4);
	}
	MarkerImage& m = g_markerImages[image];
	free(m.pixels);
	m.pixels = copy;
	m.width = copy ? width : 0;
	m.height = copy ? height : 0;
	m.isNew = true;
}

static void __cdecl Api_SubmitMarkers(const Avp2XrMarker* markers, int count)
{
	g_markerCount = 0;
	if (!g_frameBegun || !g_viewsValid || !markers || count <= 0)
		return;
	if (count > AVP2XR_MAX_MARKERS)
		count = AVP2XR_MAX_MARKERS;
	memcpy(g_markers, markers, count * sizeof(Avp2XrMarker));
	g_markerCount = count;
}

static const Avp2XrApi g_api = { AVP2XR_API_VERSION, Api_BeginStereoFrame, Api_SubmitStereo, Api_SetGameResolution, Api_GetInput,
	Api_SetCrosshairImage, Api_SubmitCrosshairs, Api_SetMarkerImage, Api_SubmitMarkers };

extern "C" const Avp2XrApi* __cdecl avp2xr_GetApi()
{
	return &g_api;
}

static XrRect2Di EyeRectPixels(const float rect[4])
{
	XrRect2Di r;
	r.offset.x = (int32_t)(rect[0] * g_scWidth + 0.5f);
	r.offset.y = (int32_t)(rect[1] * g_scHeight + 0.5f);
	r.extent.width = (int32_t)(rect[2] * g_scWidth + 0.5f);
	r.extent.height = (int32_t)(rect[3] * g_scHeight + 0.5f);
	return r;
}

// ---------------------------------------------------------------------------------------------
// HUD panel
//
// In stereo the game flips twice per frame: first the two eye views, then the HUD alone on a
// black screen. The HUD image is turned into a transparent overlay (alpha = brightest channel,
// so black disappears) and shown as a head-locked quad (VIEW space) in front of the viewer.
// The game draws the HUD at its own resolution and dgVoodoo only stretches it up (unevenly, to
// the forced resolution), so each game pixel is read back exactly from the middle of its
// stretched block. The panel image is then made at about the headset's pixel density where the
// quad is shown, each pixel the area-weighted average of the game pixels it covers (in linear
// light). The compositor then maps it about 1:1; given the game's pixels as they are (~1.2
// headset pixels each) it would draw some one pixel wide and some two, making jagged strokes.
//
// Flat frames (menus, loading screens) are made the same way into a screen panel, opaque. Handing
// the compositor the whole stretched back buffer instead makes it shrink ~3.5x on the fly with no
// mipmaps, which skips texels: thin strokes of the menu text drop out.

static const char g_hudShaderSource[] =
	"Texture2D hudImage : register(t0);\n"
	"cbuffer Params : register(b0)\n"
	"{\n"
	"	float2 srcPerGame;\n"  // back buffer pixels per game pixel (dgVoodoo's stretch)
	"	float2 gamePerDst;\n"  // game pixels per panel pixel
	"	float2 gameSize;\n"
	"	float linearize;\n"    // the target is sRGB: write linear colour
	"	float opaque;\n"       // alpha 1, else the brightest channel (black is transparent)
	"};\n"
	"float4 VSMain(uint id : SV_VertexID) : SV_Position\n"
	"{\n"
	"	float2 uv = float2((id << 1) & 2, id & 2);\n"
	"	return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);\n"
	"}\n"
	"float4 GamePixel(int2 g)\n"  // linear, premultiplied (black adds nothing)
	"{\n"
	"	float3 c = hudImage.Load(int3((g + 0.5) * srcPerGame, 0)).rgb;\n"
	"	float a = opaque > 0.5 ? 1 : max(c.r, max(c.g, c.b));\n"
	"	return float4(c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4), a);\n"
	"}\n"
	"float4 PSMain(float4 pos : SV_Position) : SV_Target\n"
	"{\n"
	"	float2 lo = (pos.xy - 0.5) * gamePerDst;\n"  // the game pixels this panel pixel covers
	"	float2 hi = min(lo + gamePerDst, gameSize);\n"
	"	int2 first = (int2)floor(lo);\n"
	"	int2 last = (int2)ceil(hi) - 1;\n"
	"	float4 sum = 0;\n"
	"	float total = 0;\n"
	"	[loop] for (int j = 0; j < 4; ++j)\n"  // footprints span at most 3 game pixels (PanelScale >= 0.5)
	"	{\n"
	"		int y = first.y + j;\n"
	"		if (y > last.y) break;\n"
	"		float wy = min(hi.y, y + 1) - max(lo.y, y);\n"
	"		[loop] for (int i = 0; i < 4; ++i)\n"
	"		{\n"
	"			int x = first.x + i;\n"
	"			if (x > last.x) break;\n"
	"			float w = wy * (min(hi.x, x + 1) - max(lo.x, x));\n"
	"			sum += GamePixel(int2(x, y)) * w;\n"
	"			total += w;\n"
	"		}\n"
	"	}\n"
	"	float4 c = sum / max(total, 1e-6);\n"
	"	float3 enc = c.rgb <= 0.0031308 ? c.rgb * 12.92 : 1.055 * pow(c.rgb, 1 / 2.4) - 0.055;\n"
	"	return float4(linearize > 0.5 ? c.rgb : enc, c.a);\n"
	"}\n";

static ID3D11DeviceContext1* g_context1;
static ID3DDeviceContextState* g_hudState;  // our own pipeline state, so dgVoodoo's is untouched
static ID3D11VertexShader* g_hudVs;
static ID3D11PixelShader* g_hudPs;
static bool g_hudBroken;  // setup failed once; stereo then runs without the HUD, menus as stretched

// A quad layer made at the game's resolution from a back buffer dgVoodoo stretched
struct Panel
{
	const char* name;
	bool opaque;                     // alpha 1 (the menu screen), or the brightest channel (the HUD)
	XrSwapchain swapchain;
	UINT width, height;              // the swapchain (the game's resolution)
	UINT sourceWidth, sourceHeight;  // the back buffer it's made from
	DXGI_FORMAT sourceFormat;
	ID3D11Texture2D* copy;           // the back buffer can't be sampled directly
	ID3D11ShaderResourceView* srv;
	ID3D11RenderTargetView** rtvs;
	uint32_t imageCount;
	ID3D11Buffer* params;
};
static Panel g_hud = { "HUD", false }, g_screen = { "Screen", true };

// Headset pixels per unit of tangent across the middle of the view, from the eye image size the
// runtime recommends and its field of view (fixed, so found once). 0 until known.
static float DisplayPixelsPerTan()
{
	static float perTan;
	if (perTan > 0 || !g_recommendedEyeWidth || !g_frameBegun)
		return perTan;
	XrViewLocateInfo li = { XR_TYPE_VIEW_LOCATE_INFO };
	li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	li.displayTime = g_frameState.predictedDisplayTime;
	li.space = g_space;
	XrViewState vs = { XR_TYPE_VIEW_STATE };
	XrView views[2] = { { XR_TYPE_VIEW }, { XR_TYPE_VIEW } };
	uint32_t count = 0;
	if (XR_FAILED(x_xrLocateViews(g_session, &li, &vs, 2, &count, views)) || count != 2)
		return 0;
	float span = tanf(views[0].fov.angleRight) - tanf(views[0].fov.angleLeft);
	if (span <= 0.1f)
		return 0;
	perTan = g_recommendedEyeWidth / span;
	Log("Headset: %.0f pixels per unit of tangent (%.1f per degree in the middle of the view)", perTan, perTan * 0.017455f);
	return perTan;
}

template <class T> static void SafeRelease(T*& p)
{
	if (p)
		p->Release();
	p = nullptr;
}

static void DestroyPanelTargets(Panel& p)
{
	for (uint32_t i = 0; i < p.imageCount; ++i)
		SafeRelease(p.rtvs[i]);
	delete[] p.rtvs;
	p.rtvs = nullptr;
	p.imageCount = 0;
	if (p.swapchain)
		x_xrDestroySwapchain(p.swapchain);
	p.swapchain = XR_NULL_HANDLE;
	SafeRelease(p.srv);
	SafeRelease(p.copy);
	p.width = p.height = p.sourceWidth = p.sourceHeight = 0;
}

static void DestroyHud()
{
	DestroyPanelTargets(g_hud);
	DestroyPanelTargets(g_screen);
	SafeRelease(g_hud.params);
	SafeRelease(g_screen.params);
	SafeRelease(g_hudPs);
	SafeRelease(g_hudVs);
	SafeRelease(g_hudState);
	SafeRelease(g_context1);
	g_hudBroken = false;
}

static bool CreateHudPipeline()
{
	HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
	pD3DCompile compile = compiler ? (pD3DCompile)GetProcAddress(compiler, "D3DCompile") : nullptr;
	if (!compile)
	{
		Log("HUD: d3dcompiler_47.dll not available");
		return false;
	}

	ID3DBlob* vs = nullptr;
	ID3DBlob* ps = nullptr;
	ID3DBlob* errors = nullptr;
	bool ok = false;
	if (FAILED(compile(g_hudShaderSource, sizeof(g_hudShaderSource) - 1, "avp2xr_hud", nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vs, &errors)) ||
		FAILED(compile(g_hudShaderSource, sizeof(g_hudShaderSource) - 1, "avp2xr_hud", nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &ps, &errors)))
	{
		Log("HUD: shader compile failed: %s", errors ? (const char*)errors->GetBufferPointer() : "?");
	}
	else if (FAILED(g_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_hudVs)) ||
		FAILED(g_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &g_hudPs)))
	{
		Log("HUD: shader creation failed");
	}
	else
	{
		ID3D11Device1* device1 = nullptr;
		D3D_FEATURE_LEVEL level = g_device->GetFeatureLevel();
		if (FAILED(g_device->QueryInterface(IID_PPV_ARGS(&device1))) || FAILED(g_context->QueryInterface(IID_PPV_ARGS(&g_context1))))
			Log("HUD: D3D11.1 isn't available");
		else if (FAILED(device1->CreateDeviceContextState(0, &level, 1, D3D11_SDK_VERSION, __uuidof(ID3D11Device1), nullptr, &g_hudState)))
			Log("HUD: CreateDeviceContextState failed");
		else
			ok = true;
		SafeRelease(device1);
	}
	SafeRelease(vs);
	SafeRelease(ps);
	SafeRelease(errors);
	return ok;
}

static bool IsSrgb(DXGI_FORMAT f)
{
	return f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || f == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
}

// ---------------------------------------------------------------------------------------------
// Monitor mirror of one eye
//
// The monitor shows one eye, cropped to the back buffer's shape and scaled up, so onlookers see a
// single normal view, with the HUD on top. The eye image is kept when the eyes are flipped; when
// the HUD image follows, the back buffer is redrawn with the eye and the HUD blended over it (as
// in the headset: alpha = brightest channel) and that is presented. It uses the HUD's own
// pipeline state, so dgVoodoo's is untouched.

static const char g_mirrorShaderSource[] =
	"Texture2D image : register(t0);\n"
	"SamplerState linearClamp : register(s0);\n"
	"cbuffer Params : register(b0) { float4 srcRect; };\n"  // u, v, width, height of the source area
	"struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
	"VSOut VSMain(uint id : SV_VertexID)\n"
	"{\n"
	"	float2 t = float2((id << 1) & 2, id & 2);\n"
	"	VSOut o;\n"
	"	o.pos = float4(t * float2(2, -2) + float2(-1, 1), 0, 1);\n"
	"	o.uv = srcRect.xy + t * srcRect.zw;\n"
	"	return o;\n"
	"}\n"
	"float4 PSMain(VSOut i) : SV_Target\n"
	"{\n"
	"	return float4(image.Sample(linearClamp, i.uv).rgb, 1);\n"
	"}\n"
	"float4 PSHud(VSOut i) : SV_Target\n"
	"{\n"
	"	float3 c = image.Sample(linearClamp, i.uv).rgb;\n"
	"	return float4(c, max(c.r, max(c.g, c.b)));\n"  // premultiplied: black adds nothing
	"}\n";

static ID3D11VertexShader* g_mirrorVs;
static ID3D11PixelShader* g_mirrorPs;
static ID3D11PixelShader* g_mirrorHudPs;
static ID3D11SamplerState* g_mirrorSampler;
static ID3D11BlendState* g_mirrorHudBlend;
static ID3D11Buffer* g_mirrorParams;
static ID3D11Texture2D* g_mirrorCopy;  // the eye image (the whole back buffer at the eye flip)
static ID3D11ShaderResourceView* g_mirrorSrv;
static bool g_mirrorBroken;   // setup failed once; the monitor then shows both eyes
static bool g_mirrorPending;  // the eye image is kept for the HUD flip to present

static void DestroyMirror()
{
	SafeRelease(g_mirrorSrv);
	SafeRelease(g_mirrorCopy);
	SafeRelease(g_mirrorParams);
	SafeRelease(g_mirrorHudBlend);
	SafeRelease(g_mirrorSampler);
	SafeRelease(g_mirrorHudPs);
	SafeRelease(g_mirrorPs);
	SafeRelease(g_mirrorVs);
	g_mirrorBroken = g_mirrorPending = false;
}

static bool CreateMirrorPipeline()
{
	HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
	pD3DCompile compile = compiler ? (pD3DCompile)GetProcAddress(compiler, "D3DCompile") : nullptr;
	if (!compile)
		return false;

	ID3DBlob* vs = nullptr;
	ID3DBlob* ps = nullptr;
	ID3DBlob* psHud = nullptr;
	ID3DBlob* errors = nullptr;
	bool ok = false;
	if (FAILED(compile(g_mirrorShaderSource, sizeof(g_mirrorShaderSource) - 1, "avp2xr_mirror", nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vs, &errors)) ||
		FAILED(compile(g_mirrorShaderSource, sizeof(g_mirrorShaderSource) - 1, "avp2xr_mirror", nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &ps, &errors)) ||
		FAILED(compile(g_mirrorShaderSource, sizeof(g_mirrorShaderSource) - 1, "avp2xr_mirror", nullptr, nullptr, "PSHud", "ps_4_0", 0, 0, &psHud, &errors)))
	{
		Log("Mirror: shader compile failed: %s", errors ? (const char*)errors->GetBufferPointer() : "?");
	}
	else if (FAILED(g_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_mirrorVs)) ||
		FAILED(g_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &g_mirrorPs)) ||
		FAILED(g_device->CreatePixelShader(psHud->GetBufferPointer(), psHud->GetBufferSize(), nullptr, &g_mirrorHudPs)))
	{
		Log("Mirror: shader creation failed");
	}
	else
	{
		D3D11_SAMPLER_DESC sd = {};
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.MaxLOD = D3D11_FLOAT32_MAX;
		D3D11_BLEND_DESC bld = {};
		bld.RenderTarget[0].BlendEnable = TRUE;
		bld.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		bld.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bld.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bld.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		bld.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		bld.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth = 16;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(g_device->CreateSamplerState(&sd, &g_mirrorSampler)) || FAILED(g_device->CreateBlendState(&bld, &g_mirrorHudBlend)) ||
			FAILED(g_device->CreateBuffer(&bd, nullptr, &g_mirrorParams)))
			Log("Mirror: sampler, blend state or constant buffer creation failed");
		else
			ok = true;
	}
	SafeRelease(vs);
	SafeRelease(ps);
	SafeRelease(psHud);
	SafeRelease(errors);
	return ok;
}

// Checks the mirror can work on this back buffer, setting it up the first time.
static bool MirrorReady(const D3D11_TEXTURE2D_DESC& bb)
{
	if (g_mirrorBroken)
		return false;
	if (((!g_hudState || !g_context1) && !CreateHudPipeline()) || (!g_mirrorVs && !CreateMirrorPipeline()))
	{
		Log("Mirror: disabled; the monitor shows both eyes");
		DestroyMirror();
		g_mirrorBroken = true;
		return false;
	}
	if (bb.SampleDesc.Count != 1 || !(bb.BindFlags & D3D11_BIND_RENDER_TARGET))
	{
		Log("Mirror: the back buffer can't be drawn to (samples %u, bind flags %x); the monitor shows both eyes", bb.SampleDesc.Count, bb.BindFlags);
		g_mirrorBroken = true;
		return false;
	}

	D3D11_TEXTURE2D_DESC cd = {};
	if (g_mirrorCopy)
		g_mirrorCopy->GetDesc(&cd);
	if (!g_mirrorCopy || cd.Width != bb.Width || cd.Height != bb.Height || cd.Format != bb.Format)
	{
		SafeRelease(g_mirrorSrv);
		SafeRelease(g_mirrorCopy);
		cd = bb;
		cd.MipLevels = 1;
		cd.ArraySize = 1;
		cd.Usage = D3D11_USAGE_DEFAULT;
		cd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		cd.CPUAccessFlags = 0;
		cd.MiscFlags = 0;
		if (FAILED(g_device->CreateTexture2D(&cd, nullptr, &g_mirrorCopy)) || FAILED(g_device->CreateShaderResourceView(g_mirrorCopy, nullptr, &g_mirrorSrv)))
		{
			Log("Mirror: can't create a sampleable copy of back buffer format %d; the monitor shows both eyes", bb.Format);
			SafeRelease(g_mirrorSrv);
			SafeRelease(g_mirrorCopy);
			g_mirrorBroken = true;
			return false;
		}
	}
	return true;
}

// Keeps the eye image (the back buffer at the eye flip) for MirrorDraw.
static bool MirrorKeepEyes(IDXGISwapChain* sc)
{
	ID3D11Texture2D* backBuffer = nullptr;
	if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
		return false;
	D3D11_TEXTURE2D_DESC bb;
	backBuffer->GetDesc(&bb);
	bool ok = MirrorReady(bb);
	if (ok)
		g_context->CopyResource(g_mirrorCopy, backBuffer);
	backBuffer->Release();
	return ok;
}

static void MirrorPass(ID3D11PixelShader* ps, ID3D11ShaderResourceView* srv, const float srcRect[4], const D3D11_VIEWPORT& vp, ID3D11BlendState* blend)
{
	g_context->UpdateSubresource(g_mirrorParams, 0, nullptr, srcRect, 0, 0);
	g_context1->RSSetViewports(1, &vp);
	g_context1->OMSetBlendState(blend, nullptr, 0xffffffff);
	g_context1->PSSetShader(ps, nullptr, 0);
	g_context1->PSSetShaderResources(0, 1, &srv);
	g_context1->Draw(3, 0);
}

// Redraws the back buffer with the kept eye image (eye 0 left, 1 right) filling it, and with
// hud (the HUD image, the game's screen stretched over the back buffer) over it in the game's
// aspect ratio, if given.
static bool MirrorDraw(IDXGISwapChain* sc, int eye, ID3D11ShaderResourceView* hud)
{
	ID3D11Texture2D* backBuffer = nullptr;
	if (!g_mirrorSrv || FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
		return false;
	D3D11_TEXTURE2D_DESC bb;
	backBuffer->GetDesc(&bb);
	ID3D11RenderTargetView* rtv = nullptr;
	if (FAILED(g_device->CreateRenderTargetView(backBuffer, nullptr, &rtv)))
	{
		backBuffer->Release();
		return false;
	}

	// The eye's rectangle cropped to the back buffer's aspect ratio, about its centre
	const float* r = g_eyeSubmit[eye].rect;
	float aspect = (float)bb.Width / (float)bb.Height;
	float eyeW = r[2] * bb.Width, eyeH = r[3] * bb.Height;
	float eyeRect[4] = { r[0], r[1], r[2], r[3] };
	if (eyeW / eyeH < aspect)
	{
		eyeRect[3] = eyeW / aspect / bb.Height;
		eyeRect[1] = r[1] + (r[3] - eyeRect[3]) * 0.5f;
	}
	else
	{
		eyeRect[2] = eyeH * aspect / bb.Width;
		eyeRect[0] = r[0] + (r[2] - eyeRect[2]) * 0.5f;
	}

	ID3DDeviceContextState* previous = nullptr;
	g_context1->SwapDeviceContextState(g_hudState, &previous);

	g_context1->OMSetRenderTargets(1, &rtv, nullptr);
	g_context1->IASetInputLayout(nullptr);
	g_context1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context1->VSSetShader(g_mirrorVs, nullptr, 0);
	g_context1->VSSetConstantBuffers(0, 1, &g_mirrorParams);
	g_context1->PSSetSamplers(0, 1, &g_mirrorSampler);

	D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)bb.Width, (float)bb.Height, 0.0f, 1.0f };
	MirrorPass(g_mirrorPs, g_mirrorSrv, eyeRect, vp, nullptr);

	if (hud)
	{
		// The HUD is the game's whole screen, so it goes in the game's aspect ratio, centred
		float gameAspect = (g_gameWidth > 0 && g_gameHeight > 0) ? (float)g_gameWidth / (float)g_gameHeight : 4.0f / 3.0f;
		D3D11_VIEWPORT hv = vp;
		if (gameAspect < aspect)
		{
			hv.Width = bb.Height * gameAspect;
			hv.TopLeftX = (bb.Width - hv.Width) * 0.5f;
		}
		else
		{
			hv.Height = bb.Width / gameAspect;
			hv.TopLeftY = (bb.Height - hv.Height) * 0.5f;
		}
		const float whole[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
		MirrorPass(g_mirrorHudPs, hud, whole, hv, g_mirrorHudBlend);
	}

	ID3D11ShaderResourceView* noSrv = nullptr;
	g_context1->PSSetShaderResources(0, 1, &noSrv);
	g_context1->OMSetBlendState(nullptr, nullptr, 0xffffffff);  // the HUD capture shares this state
	g_context1->OMSetRenderTargets(0, nullptr, nullptr);
	g_context1->SwapDeviceContextState(previous, nullptr);
	SafeRelease(previous);

	rtv->Release();
	backBuffer->Release();
	return true;
}

// Flat frames (menus, loading screens): dgVoodoo stretches the game's screen over the whole back
// buffer, so on the monitor it's redrawn in the game's aspect ratio, centred, with black bars.
// Call after the frame has been copied for the headset.
static bool MirrorFlat(IDXGISwapChain* sc)
{
	if (g_gameWidth <= 0 || g_gameHeight <= 0 || !MirrorKeepEyes(sc))
		return false;

	ID3D11Texture2D* backBuffer = nullptr;
	if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
		return false;
	D3D11_TEXTURE2D_DESC bb;
	backBuffer->GetDesc(&bb);
	ID3D11RenderTargetView* rtv = nullptr;
	if (FAILED(g_device->CreateRenderTargetView(backBuffer, nullptr, &rtv)))
	{
		backBuffer->Release();
		return false;
	}

	float aspect = (float)bb.Width / (float)bb.Height;
	float gameAspect = (float)g_gameWidth / (float)g_gameHeight;
	D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)bb.Width, (float)bb.Height, 0.0f, 1.0f };
	if (gameAspect < aspect)
	{
		vp.Width = bb.Height * gameAspect;
		vp.TopLeftX = (bb.Width - vp.Width) * 0.5f;
	}
	else
	{
		vp.Height = bb.Width / gameAspect;
		vp.TopLeftY = (bb.Height - vp.Height) * 0.5f;
	}

	ID3DDeviceContextState* previous = nullptr;
	g_context1->SwapDeviceContextState(g_hudState, &previous);

	const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	g_context1->ClearRenderTargetView(rtv, black);
	g_context1->OMSetRenderTargets(1, &rtv, nullptr);
	g_context1->IASetInputLayout(nullptr);
	g_context1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context1->VSSetShader(g_mirrorVs, nullptr, 0);
	g_context1->VSSetConstantBuffers(0, 1, &g_mirrorParams);
	g_context1->PSSetSamplers(0, 1, &g_mirrorSampler);
	const float whole[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	MirrorPass(g_mirrorPs, g_mirrorSrv, whole, vp, nullptr);

	ID3D11ShaderResourceView* noSrv = nullptr;
	g_context1->PSSetShaderResources(0, 1, &noSrv);
	g_context1->OMSetRenderTargets(0, nullptr, nullptr);
	g_context1->SwapDeviceContextState(previous, nullptr);
	SafeRelease(previous);

	static bool logged;
	if (!logged)
	{
		logged = true;
		Log("Mirror: flat frames shown at %dx%d in %.0fx%.0f at %.0f,%.0f of the %ux%u back buffer",
			g_gameWidth, g_gameHeight, vp.Width, vp.Height, vp.TopLeftX, vp.TopLeftY, bb.Width, bb.Height);
	}

	rtv->Release();
	backBuffer->Release();
	return true;
}

// Any 8-bit RGBA format will do since the HUD is drawn with a shader, not copied.
static DXGI_FORMAT ChooseHudFormat()
{
	uint32_t count = 0;
	if (XR_FAILED(x_xrEnumerateSwapchainFormats(g_session, 0, &count, nullptr)) || !count)
		return DXGI_FORMAT_UNKNOWN;
	int64_t* formats = new int64_t[count];
	x_xrEnumerateSwapchainFormats(g_session, count, &count, formats);
	const DXGI_FORMAT wanted[] = { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
		DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM };
	DXGI_FORMAT chosen = DXGI_FORMAT_UNKNOWN;
	for (int w = 0; w < 4 && chosen == DXGI_FORMAT_UNKNOWN; ++w)
		for (uint32_t i = 0; i < count; ++i)
			if (formats[i] == wanted[w]) { chosen = wanted[w]; break; }
	delete[] formats;
	return chosen;
}

static bool EnsurePanel(Panel& p, const D3D11_TEXTURE2D_DESC& bb)
{
	if (g_hudBroken)
		return false;
	if (!g_hudState && !CreateHudPipeline())
	{
		Log("HUD disabled; stereo continues without it and menus are shown stretched");
		DestroyHud();
		g_hudBroken = true;
		return false;
	}
	// The game's pixels (read back from dgVoodoo's stretch), or the back buffer as is if unknown
	UINT gameWidth = bb.Width, gameHeight = bb.Height;
	if (g_gameWidth > 0 && g_gameHeight > 0 && (UINT)g_gameWidth <= gameWidth && (UINT)g_gameHeight <= gameHeight)
	{
		gameWidth = (UINT)g_gameWidth;
		gameHeight = (UINT)g_gameHeight;
	}
	// The panel image: the headset's pixel density across the quad, times PanelScale
	UINT width = gameWidth, height = gameHeight;
	float perTan = DisplayPixelsPerTan();
	float quadWidth = &p == &g_hud ? g_cfg.hudWidth : g_cfg.width;
	float quadDistance = &p == &g_hud ? g_cfg.hudDistance : g_cfg.distance;
	if (g_cfg.panelScale > 0 && perTan > 0 && quadDistance > 0.01f)
	{
		float scale = g_cfg.panelScale * perTan * quadWidth / quadDistance / gameWidth;
		scale = scale < 0.5f ? 0.5f : scale > 3.0f ? 3.0f : scale;
		width = (UINT)(gameWidth * scale + 0.5f);
		height = (UINT)(gameHeight * scale + 0.5f);
		if (width > 4096 || height > 4096)
		{
			float fit = 4096.0f / (width > height ? width : height);
			width = (UINT)(width * fit);
			height = (UINT)(height * fit);
		}
	}
	if (p.swapchain && p.sourceWidth == bb.Width && p.sourceHeight == bb.Height && p.sourceFormat == bb.Format &&
		p.width == width && p.height == height)
		return true;
	DestroyPanelTargets(p);

	if (!p.params)
	{
		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth = 32;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(g_device->CreateBuffer(&bd, nullptr, &p.params)))
		{
			Log("%s: constant buffer creation failed", p.name);
			return false;
		}
	}

	D3D11_TEXTURE2D_DESC cd = {};
	cd.Width = bb.Width;
	cd.Height = bb.Height;
	cd.MipLevels = 1;
	cd.ArraySize = 1;
	cd.Format = bb.Format;
	cd.SampleDesc.Count = 1;
	cd.Usage = D3D11_USAGE_DEFAULT;
	cd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (FAILED(g_device->CreateTexture2D(&cd, nullptr, &p.copy)) || FAILED(g_device->CreateShaderResourceView(p.copy, nullptr, &p.srv)))
	{
		Log("%s: can't create a sampleable copy of back buffer format %d", p.name, bb.Format);
		DestroyPanelTargets(p);
		return false;
	}

	DXGI_FORMAT format = ChooseHudFormat();
	if (format == DXGI_FORMAT_UNKNOWN)
	{
		Log("%s: the runtime offers no 8-bit RGBA swapchain format", p.name);
		DestroyPanelTargets(p);
		return false;
	}
	XrSwapchainCreateInfo ci = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
	ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
	ci.format = format;
	ci.sampleCount = 1;
	ci.width = width;
	ci.height = height;
	ci.faceCount = 1;
	ci.arraySize = 1;
	ci.mipCount = 1;
	XrResult r = x_xrCreateSwapchain(g_session, &ci, &p.swapchain);
	if (XR_FAILED(r))
	{
		Log("%s: xrCreateSwapchain failed: %s", p.name, XrStr(r));
		p.swapchain = XR_NULL_HANDLE;
		DestroyPanelTargets(p);
		return false;
	}

	x_xrEnumerateSwapchainImages(p.swapchain, 0, &p.imageCount, nullptr);
	XrSwapchainImageD3D11KHR* images = new XrSwapchainImageD3D11KHR[p.imageCount];
	for (uint32_t i = 0; i < p.imageCount; ++i)
	{
		images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
		images[i].next = nullptr;
	}
	x_xrEnumerateSwapchainImages(p.swapchain, p.imageCount, &p.imageCount, (XrSwapchainImageBaseHeader*)images);
	p.rtvs = new ID3D11RenderTargetView*[p.imageCount]();
	bool ok = true;
	for (uint32_t i = 0; i < p.imageCount && ok; ++i)
	{
		D3D11_RENDER_TARGET_VIEW_DESC rd = {};
		rd.Format = format;
		rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		ok = SUCCEEDED(g_device->CreateRenderTargetView(images[i].texture, &rd, &p.rtvs[i]));
	}
	delete[] images;
	if (!ok)
	{
		Log("%s: can't create render targets for the XR swapchain", p.name);
		DestroyPanelTargets(p);
		return false;
	}

	// An sRGB target re-encodes what the shader writes, so the shader has to output linear colour.
	float params[8] = { (float)bb.Width / gameWidth, (float)bb.Height / gameHeight, (float)gameWidth / width, (float)gameHeight / height,
		(float)gameWidth, (float)gameHeight, IsSrgb(format) ? 1.0f : 0.0f, p.opaque ? 1.0f : 0.0f };
	g_context->UpdateSubresource(p.params, 0, nullptr, params, 0, 0);

	p.width = width;
	p.height = height;
	p.sourceWidth = bb.Width;
	p.sourceHeight = bb.Height;
	p.sourceFormat = bb.Format;
	Log("%s swapchain %ux%u (from the game's %ux%u in a %ux%u image), format %d, %u images", p.name, width, height,
		gameWidth, gameHeight, bb.Width, bb.Height, format, p.imageCount);
	return true;
}

// Debugging aid (AVP2XR_DUMP=<folder>): each panel's 60th image is written to the folder, both
// the back buffer it's made from (<panel>_source.raw) and what the headset gets (<panel>.raw):
// width and height as two uint32, then 4 bytes a pixel.
static void DumpTexture(ID3D11Resource* texture, const wchar_t* dir, const char* name)
{
	ID3D11Texture2D* tex = nullptr;
	if (FAILED(texture->QueryInterface(IID_PPV_ARGS(&tex))))
		return;
	D3D11_TEXTURE2D_DESC sd;
	tex->GetDesc(&sd);
	tex->Release();
	sd.MipLevels = sd.ArraySize = 1;
	sd.BindFlags = sd.MiscFlags = 0;
	sd.Usage = D3D11_USAGE_STAGING;
	sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ID3D11Texture2D* staging = nullptr;
	if (FAILED(g_device->CreateTexture2D(&sd, nullptr, &staging)))
		return;
	g_context->CopySubresourceRegion(staging, 0, 0, 0, 0, texture, 0, nullptr);
	D3D11_MAPPED_SUBRESOURCE m;
	if (SUCCEEDED(g_context->Map(staging, 0, D3D11_MAP_READ, 0, &m)))
	{
		wchar_t path[MAX_PATH];
		swprintf_s(path, L"%s\\%hs.raw", dir, name);
		FILE* f = nullptr;
		if (!_wfopen_s(&f, path, L"wb") && f)
		{
			uint32_t size[2] = { sd.Width, sd.Height };
			fwrite(size, sizeof(size), 1, f);
			for (UINT y = 0; y < sd.Height; ++y)
				fwrite((const char*)m.pData + y * m.RowPitch, 4, sd.Width, f);
			fclose(f);
			Log("Dumped %s (%ux%u)", name, sd.Width, sd.Height);
		}
		g_context->Unmap(staging, 0);
	}
	SafeRelease(staging);
}

static void DumpPanel(Panel& p, ID3D11RenderTargetView* image)
{
	static wchar_t dir[MAX_PATH];
	static const bool dump = GetEnvironmentVariableW(L"AVP2XR_DUMP", dir, MAX_PATH) != 0;
	static int hudCount, screenCount;
	int& count = &p == &g_hud ? hudCount : screenCount;
	if (!dump || ++count != 60)
		return;
	char name[64];
	sprintf_s(name, "%s_source", p.name);
	DumpTexture(p.copy, dir, name);
	ID3D11Resource* res = nullptr;
	image->GetResource(&res);
	if (res)
		DumpTexture(res, dir, p.name);
	SafeRelease(res);
}

// Turns the current back buffer into the next image of the panel's swapchain.
static bool CapturePanel(Panel& p, IDXGISwapChain* sc)
{
	ID3D11Texture2D* backBuffer = nullptr;
	if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
		return false;
	D3D11_TEXTURE2D_DESC bb;
	backBuffer->GetDesc(&bb);

	bool ok = false;
	if (EnsurePanel(p, bb))
	{
		if (bb.SampleDesc.Count > 1)
			g_context->ResolveSubresource(p.copy, 0, backBuffer, 0, bb.Format);
		else
			g_context->CopyResource(p.copy, backBuffer);

		uint32_t index;
		XrSwapchainImageAcquireInfo ai = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
		XrSwapchainImageWaitInfo wi = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
		wi.timeout = XR_INFINITE_DURATION;
		if (XR_SUCCEEDED(x_xrAcquireSwapchainImage(p.swapchain, &ai, &index)) &&
			XR_SUCCEEDED(x_xrWaitSwapchainImage(p.swapchain, &wi)))
		{
			ID3DDeviceContextState* previous = nullptr;
			g_context1->SwapDeviceContextState(g_hudState, &previous);

			D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)p.width, (float)p.height, 0.0f, 1.0f };
			g_context1->OMSetRenderTargets(1, &p.rtvs[index], nullptr);
			g_context1->OMSetBlendState(nullptr, nullptr, 0xffffffff);  // written as is, no blending
			g_context1->RSSetViewports(1, &vp);
			g_context1->IASetInputLayout(nullptr);
			g_context1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			g_context1->VSSetShader(g_hudVs, nullptr, 0);
			g_context1->PSSetShader(g_hudPs, nullptr, 0);
			g_context1->PSSetShaderResources(0, 1, &p.srv);
			g_context1->PSSetConstantBuffers(0, 1, &p.params);
			g_context1->Draw(3, 0);
			ID3D11ShaderResourceView* noSrv = nullptr;
			g_context1->PSSetShaderResources(0, 1, &noSrv);
			g_context1->OMSetRenderTargets(0, nullptr, nullptr);

			g_context1->SwapDeviceContextState(previous, nullptr);
			SafeRelease(previous);
			DumpPanel(p, p.rtvs[index]);

			// Debugging aid (AVP2XR_HUDCHECK set): every couple of seconds, log the panel image's
			// centre pixel as the headset gets it
			static const bool hudCheck = GetEnvironmentVariableA("AVP2XR_HUDCHECK", nullptr, 0) != 0;
			static DWORD nextCheck;
			if (hudCheck && (int)(GetTickCount() - nextCheck) >= 0)
			{
				nextCheck = GetTickCount() + 2000;
				ID3D11Resource* image = nullptr;
				p.rtvs[index]->GetResource(&image);
				D3D11_TEXTURE2D_DESC sd = {};
				sd.Width = sd.Height = 1;
				sd.MipLevels = sd.ArraySize = 1;
				sd.Format = ChooseHudFormat();
				sd.SampleDesc.Count = 1;
				sd.Usage = D3D11_USAGE_STAGING;
				sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
				ID3D11Texture2D* staging = nullptr;
				if (image && SUCCEEDED(g_device->CreateTexture2D(&sd, nullptr, &staging)))
				{
					D3D11_BOX box = { p.width / 2, p.height / 2, 0, p.width / 2 + 1, p.height / 2 + 1, 1 };
					g_context->CopySubresourceRegion(staging, 0, 0, 0, 0, image, 0, &box);
					D3D11_MAPPED_SUBRESOURCE m;
					if (SUCCEEDED(g_context->Map(staging, 0, D3D11_MAP_READ, 0, &m)))
					{
						const unsigned char* px = (const unsigned char*)m.pData;
						Log("%s check: centre pixel %u %u %u alpha %u", p.name, px[0], px[1], px[2], px[3]);
						g_context->Unmap(staging, 0);
					}
				}
				SafeRelease(staging);
				SafeRelease(image);
			}

			XrSwapchainImageReleaseInfo ri = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
			ok = XR_SUCCEEDED(x_xrReleaseSwapchainImage(p.swapchain, &ri));
		}
	}
	backBuffer->Release();
	return ok;
}

// ---------------------------------------------------------------------------------------------
// Frame submission

enum PresentAction
{
	PRESENT_AS_IS,     // XR isn't pacing frames; leave the Present alone
	PRESENT_NO_VSYNC,  // xrWaitFrame paces frames, so the desktop Present shouldn't wait too
	PRESENT_SKIP,      // the image was only for the headset (the HUD); don't show it on the monitor
};

enum OutputMode { OUTPUT_NONE = -1, OUTPUT_FLAT, OUTPUT_STEREO, OUTPUT_STEREO_HUD };

static XrCompositionLayerProjection g_projection = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
// ---------------------------------------------------------------------------------------------
// The crosshair, drawn into the eye images at the headset's resolution
//
// The game's 2D drawing works in its own pixels (an eye is 400 x 600 of them at 800 x 600, and
// dgVoodoo scales that up about 5.6 times), so a crosshair it draws moves in visible steps. From
// API version 10 the game gives the image and its exact place instead, and it's drawn here,
// filtered, over the back buffer before the eyes are copied for the headset (and kept for the
// monitor). It uses the HUD's own pipeline state, so dgVoodoo's is untouched.

static const char g_crosshairShaderSource[] =
	"Texture2D image : register(t0);\n"
	"SamplerState linearClamp : register(s0);\n"
	"cbuffer Params : register(b0) { float4 alpha; };\n"
	"struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
	"VSOut VSMain(uint id : SV_VertexID)\n"
	"{\n"
	"	float2 t = float2(id & 1, id >> 1);\n"  // a strip of 4 filling the viewport
	"	VSOut o;\n"
	"	o.pos = float4(t * float2(2, -2) + float2(-1, 1), 0, 1);\n"
	"	o.uv = t;\n"
	"	return o;\n"
	"}\n"
	"float4 PSMain(VSOut i) : SV_Target\n"
	"{\n"
	"	return image.Sample(linearClamp, i.uv) * alpha.x;\n"  // premultiplied
	"}\n";

static ID3D11VertexShader* g_crosshairVs;
static ID3D11PixelShader* g_crosshairPs;
static ID3D11SamplerState* g_crosshairSampler;
static ID3D11BlendState* g_crosshairBlend;
static ID3D11RasterizerState* g_crosshairRaster;  // scissored to the eye
static ID3D11Buffer* g_crosshairParams;
static ID3D11Texture2D* g_crosshairTexture;
static ID3D11ShaderResourceView* g_crosshairSrv;
static bool g_crosshairBroken;  // setup failed once; no crosshair from then on

// Markers (API version 11) are drawn the same way, but turned: the vertex shader takes the quad's
// corners (clip space) from the constant buffer instead of filling the viewport. They share the
// crosshair's sampler, blend and scissored rasterizer state.
static const char g_markerShaderSource[] =
	"Texture2D image : register(t0);\n"
	"SamplerState linearClamp : register(s0);\n"
	"cbuffer Params : register(b0) { float4 alpha; float4 corners01; float4 corners23; };\n"
	"struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
	"VSOut VSMain(uint id : SV_VertexID)\n"
	"{\n"
	"	float2 t = float2(id & 1, id >> 1);\n"  // a strip of 4: top-left, top-right, bottom-left, bottom-right
	"	float2 c = id == 0 ? corners01.xy : id == 1 ? corners01.zw : id == 2 ? corners23.xy : corners23.zw;\n"
	"	VSOut o;\n"
	"	o.pos = float4(c, 0, 1);\n"
	"	o.uv = t;\n"
	"	return o;\n"
	"}\n"
	"float4 PSMain(VSOut i) : SV_Target\n"
	"{\n"
	"	return image.Sample(linearClamp, i.uv) * alpha.x;\n"  // premultiplied
	"}\n";

static ID3D11VertexShader* g_markerVs;
static ID3D11PixelShader* g_markerPs;
static ID3D11Buffer* g_markerParams;
static bool g_markerBroken;  // setup failed once; no markers from then on

static void DestroyCrosshair()
{
	SafeRelease(g_crosshairSrv);
	SafeRelease(g_crosshairTexture);
	SafeRelease(g_crosshairParams);
	SafeRelease(g_crosshairRaster);
	SafeRelease(g_crosshairBlend);
	SafeRelease(g_crosshairSampler);
	SafeRelease(g_crosshairPs);
	SafeRelease(g_crosshairVs);
	g_crosshairBroken = false;
	g_crosshairImageNew = g_crosshairPixels != nullptr;
	for (MarkerImage& m : g_markerImages)
	{
		SafeRelease(m.srv);
		SafeRelease(m.texture);
		m.isNew = m.pixels != nullptr;
	}
	SafeRelease(g_markerParams);
	SafeRelease(g_markerPs);
	SafeRelease(g_markerVs);
	g_markerBroken = false;
}

static bool CreateCrosshairPipeline()
{
	HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
	pD3DCompile compile = compiler ? (pD3DCompile)GetProcAddress(compiler, "D3DCompile") : nullptr;
	if (!compile)
		return false;

	ID3DBlob* vs = nullptr;
	ID3DBlob* ps = nullptr;
	ID3DBlob* errors = nullptr;
	bool ok = false;
	if (FAILED(compile(g_crosshairShaderSource, sizeof(g_crosshairShaderSource) - 1, "avp2xr_crosshair", nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vs, &errors)) ||
		FAILED(compile(g_crosshairShaderSource, sizeof(g_crosshairShaderSource) - 1, "avp2xr_crosshair", nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &ps, &errors)))
	{
		Log("Crosshair: shader compile failed: %s", errors ? (const char*)errors->GetBufferPointer() : "?");
	}
	else if (FAILED(g_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_crosshairVs)) ||
		FAILED(g_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &g_crosshairPs)))
	{
		Log("Crosshair: shader creation failed");
	}
	else
	{
		D3D11_SAMPLER_DESC sd = {};
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.MaxLOD = D3D11_FLOAT32_MAX;
		D3D11_BLEND_DESC bld = {};
		bld.RenderTarget[0].BlendEnable = TRUE;
		bld.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		bld.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bld.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bld.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		bld.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		bld.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		D3D11_RASTERIZER_DESC rd = {};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_NONE;
		rd.DepthClipEnable = TRUE;
		rd.ScissorEnable = TRUE;
		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth = 16;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(g_device->CreateSamplerState(&sd, &g_crosshairSampler)) || FAILED(g_device->CreateBlendState(&bld, &g_crosshairBlend)) ||
			FAILED(g_device->CreateRasterizerState(&rd, &g_crosshairRaster)) || FAILED(g_device->CreateBuffer(&bd, nullptr, &g_crosshairParams)))
			Log("Crosshair: sampler, blend, rasterizer state or constant buffer creation failed");
		else
			ok = true;
	}
	SafeRelease(vs);
	SafeRelease(ps);
	SafeRelease(errors);
	return ok;
}

static bool CreateMarkerPipeline()
{
	HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
	pD3DCompile compile = compiler ? (pD3DCompile)GetProcAddress(compiler, "D3DCompile") : nullptr;
	if (!compile)
		return false;

	ID3DBlob* vs = nullptr;
	ID3DBlob* ps = nullptr;
	ID3DBlob* errors = nullptr;
	bool ok = false;
	if (FAILED(compile(g_markerShaderSource, sizeof(g_markerShaderSource) - 1, "avp2xr_marker", nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vs, &errors)) ||
		FAILED(compile(g_markerShaderSource, sizeof(g_markerShaderSource) - 1, "avp2xr_marker", nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &ps, &errors)))
	{
		Log("Markers: shader compile failed: %s", errors ? (const char*)errors->GetBufferPointer() : "?");
	}
	else if (FAILED(g_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_markerVs)) ||
		FAILED(g_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &g_markerPs)))
	{
		Log("Markers: shader creation failed");
	}
	else
	{
		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth = 48;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(g_device->CreateBuffer(&bd, nullptr, &g_markerParams)))
			Log("Markers: constant buffer creation failed");
		else
			ok = true;
	}
	SafeRelease(vs);
	SafeRelease(ps);
	SafeRelease(errors);
	return ok;
}

// A game image (0xAARRGGBB) as a texture: premultiplied, with mipmaps for when it's drawn smaller
// than it is
static bool CreateImageTexture(const unsigned int* pixels, int w, int h, const char* what,
	ID3D11Texture2D*& texture, ID3D11ShaderResourceView*& srv)
{
	unsigned int* data = (unsigned int*)malloc((size_t)w * h * 4);
	if (!data)
		return false;
	for (int i = 0; i < w * h; ++i)
	{
		unsigned int c = pixels[i], a = c >> 24;
		unsigned int r = ((c >> 16) & 255) * a / 255, g = ((c >> 8) & 255) * a / 255, b = (c & 255) * a / 255;
		data[i] = (a << 24) | (r << 16) | (g << 8) | b;
	}
	D3D11_TEXTURE2D_DESC td = {};
	td.Width = w;
	td.Height = h;
	td.MipLevels = 0;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // 0xAARRGGBB as it lies in memory
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	bool ok = SUCCEEDED(g_device->CreateTexture2D(&td, nullptr, &texture)) &&
		SUCCEEDED(g_device->CreateShaderResourceView(texture, nullptr, &srv));
	if (ok)
	{
		g_context->UpdateSubresource(texture, 0, nullptr, data, w * 4, 0);
		g_context->GenerateMips(srv);
		Log("%s: image %dx%d", what, w, h);
	}
	else
	{
		Log("%s: can't create a %dx%d texture", what, w, h);
		SafeRelease(srv);
		SafeRelease(texture);
	}
	free(data);
	return ok;
}

// Puts a new SetMarkerImage into its texture. False if there's no image.
static bool UpdateMarkerTexture(int image)
{
	MarkerImage& m = g_markerImages[image];
	if (!m.isNew)
		return m.srv != nullptr;
	m.isNew = false;
	SafeRelease(m.srv);
	SafeRelease(m.texture);
	char what[32];
	sprintf_s(what, "Marker %d", image);
	return m.pixels && CreateImageTexture(m.pixels, m.width, m.height, what, m.texture, m.srv);
}

// Puts a new SetCrosshairImage into the texture (premultiplied, with mipmaps for when it's drawn
// smaller than it is). False if there's no image.
static bool UpdateCrosshairTexture()
{
	if (!g_crosshairImageNew)
		return g_crosshairSrv != nullptr;
	g_crosshairImageNew = false;
	SafeRelease(g_crosshairSrv);
	SafeRelease(g_crosshairTexture);
	if (!g_crosshairPixels)
		return false;
	return CreateImageTexture(g_crosshairPixels, g_crosshairWidth, g_crosshairHeight, "Crosshair", g_crosshairTexture, g_crosshairSrv);
}

// ----------------------------------------------------------------------- //
// Gamma (avp2xr.ini Gamma): the stereo frame in the back buffer is copied and drawn back through
// pow(colour, 1 / Gamma), before the crosshair and before the eyes go to the headset (the monitor
// mirror shows it too). Uses the HUD's own pipeline state, like the crosshair.

static const char g_gammaShaderSource[] =
	"Texture2D image : register(t0);\n"
	"cbuffer Params : register(b0) { float4 power; };\n"
	"float4 VSMain(uint id : SV_VertexID) : SV_Position\n"
	"{\n"
	"	float2 t = float2(id & 1, id >> 1);\n"  // a strip of 4 filling the viewport
	"	return float4(t * float2(2, -2) + float2(-1, 1), 0, 1);\n"
	"}\n"
	"float4 PSMain(float4 pos : SV_Position) : SV_Target\n"
	"{\n"
	"	float4 c = image.Load(int3(pos.xy, 0));\n"
	"	return float4(pow(saturate(c.rgb), power.x), c.a);\n"
	"}\n";

static ID3D11VertexShader* g_gammaVs;
static ID3D11PixelShader* g_gammaPs;
static ID3D11Buffer* g_gammaParams;
static ID3D11Texture2D* g_gammaCopy;  // the frame before the pass, same size and format as the back buffer
static ID3D11ShaderResourceView* g_gammaSrv;
static bool g_gammaBroken;  // setup failed once; no gamma from then on

static void DestroyGamma()
{
	SafeRelease(g_gammaSrv);
	SafeRelease(g_gammaCopy);
	SafeRelease(g_gammaParams);
	SafeRelease(g_gammaPs);
	SafeRelease(g_gammaVs);
	g_gammaBroken = false;
}

static bool CreateGammaPipeline()
{
	HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
	pD3DCompile compile = compiler ? (pD3DCompile)GetProcAddress(compiler, "D3DCompile") : nullptr;
	if (!compile)
		return false;

	ID3DBlob* vs = nullptr;
	ID3DBlob* ps = nullptr;
	ID3DBlob* errors = nullptr;
	bool ok = false;
	D3D11_BUFFER_DESC bd = {};
	bd.ByteWidth = 16;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(compile(g_gammaShaderSource, sizeof(g_gammaShaderSource) - 1, "avp2xr_gamma", nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vs, &errors)) ||
		FAILED(compile(g_gammaShaderSource, sizeof(g_gammaShaderSource) - 1, "avp2xr_gamma", nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &ps, &errors)))
		Log("Gamma: shader compile failed: %s", errors ? (const char*)errors->GetBufferPointer() : "?");
	else if (FAILED(g_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_gammaVs)) ||
		FAILED(g_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &g_gammaPs)) ||
		FAILED(g_device->CreateBuffer(&bd, nullptr, &g_gammaParams)))
		Log("Gamma: shader or constant buffer creation failed");
	else
		ok = true;
	SafeRelease(vs);
	SafeRelease(ps);
	SafeRelease(errors);
	return ok;
}

static void ApplyGamma(IDXGISwapChain* sc)
{
	if (g_cfg.gamma == 1.0f || g_gammaBroken)
		return;
	if (((!g_hudState || !g_context1) && !CreateHudPipeline()) || (!g_gammaVs && !CreateGammaPipeline()))
	{
		Log("Gamma: can't be applied");
		DestroyGamma();
		g_gammaBroken = true;
		return;
	}

	ID3D11Texture2D* backBuffer = nullptr;
	if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
		return;
	D3D11_TEXTURE2D_DESC bb;
	backBuffer->GetDesc(&bb);

	// The copy to read from, made again if the back buffer changes size or format
	if (g_gammaCopy)
	{
		D3D11_TEXTURE2D_DESC cd;
		g_gammaCopy->GetDesc(&cd);
		if (cd.Width != bb.Width || cd.Height != bb.Height || cd.Format != bb.Format)
		{
			SafeRelease(g_gammaSrv);
			SafeRelease(g_gammaCopy);
		}
	}
	ID3D11RenderTargetView* rtv = nullptr;
	if (!g_gammaCopy)
	{
		D3D11_TEXTURE2D_DESC cd = bb;
		cd.MipLevels = 1;
		cd.ArraySize = 1;
		cd.Usage = D3D11_USAGE_DEFAULT;
		cd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		cd.CPUAccessFlags = 0;
		cd.MiscFlags = 0;
		if (bb.SampleDesc.Count != 1 || FAILED(g_device->CreateTexture2D(&cd, nullptr, &g_gammaCopy)) ||
			FAILED(g_device->CreateShaderResourceView(g_gammaCopy, nullptr, &g_gammaSrv)))
		{
			Log("Gamma: can't copy the back buffer (format %d, samples %u)", (int)bb.Format, bb.SampleDesc.Count);
			SafeRelease(g_gammaSrv);
			SafeRelease(g_gammaCopy);
			g_gammaBroken = true;
			backBuffer->Release();
			return;
		}
		Log("Gamma: %.2f on the eye images (%ux%u)", g_cfg.gamma, bb.Width, bb.Height);
	}
	if (!(bb.BindFlags & D3D11_BIND_RENDER_TARGET) || FAILED(g_device->CreateRenderTargetView(backBuffer, nullptr, &rtv)))
	{
		Log("Gamma: the back buffer can't be drawn to (bind flags %x)", bb.BindFlags);
		g_gammaBroken = true;
		backBuffer->Release();
		return;
	}
	g_context->CopyResource(g_gammaCopy, backBuffer);

	ID3DDeviceContextState* previous = nullptr;
	g_context1->SwapDeviceContextState(g_hudState, &previous);
	g_context1->OMSetRenderTargets(1, &rtv, nullptr);
	g_context1->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	g_context1->RSSetState(nullptr);
	g_context1->IASetInputLayout(nullptr);
	g_context1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	g_context1->VSSetShader(g_gammaVs, nullptr, 0);
	g_context1->PSSetShader(g_gammaPs, nullptr, 0);
	g_context1->PSSetShaderResources(0, 1, &g_gammaSrv);
	g_context1->PSSetConstantBuffers(0, 1, &g_gammaParams);
	float params[4] = { 1.0f / g_cfg.gamma, 0.0f, 0.0f, 0.0f };
	g_context1->UpdateSubresource(g_gammaParams, 0, nullptr, params, 0, 0);
	D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)bb.Width, (float)bb.Height, 0.0f, 1.0f };
	g_context1->RSSetViewports(1, &vp);
	g_context1->Draw(4, 0);
	ID3D11ShaderResourceView* noSrv = nullptr;
	g_context1->PSSetShaderResources(0, 1, &noSrv);
	g_context1->OMSetRenderTargets(0, nullptr, nullptr);
	g_context1->SwapDeviceContextState(previous, nullptr);
	SafeRelease(previous);
	rtv->Release();
	backBuffer->Release();
}

// Draws the crosshairs the game gave for this frame over the eyes in the back buffer
static void DrawCrosshairs(IDXGISwapChain* sc)
{
	Avp2XrCrosshair xh[2] = { g_crosshairs[0], g_crosshairs[1] };
	memset(g_crosshairs, 0, sizeof(g_crosshairs));
	if ((!xh[0].visible && !xh[1].visible) || g_crosshairBroken)
		return;
	if (((!g_hudState || !g_context1) && !CreateHudPipeline()) || (!g_crosshairVs && !CreateCrosshairPipeline()))
	{
		Log("Crosshair: can't be drawn");
		DestroyCrosshair();
		g_crosshairBroken = true;
		return;
	}
	if (!UpdateCrosshairTexture())
		return;

	ID3D11Texture2D* backBuffer = nullptr;
	if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
		return;
	D3D11_TEXTURE2D_DESC bb;
	backBuffer->GetDesc(&bb);
	ID3D11RenderTargetView* rtv = nullptr;
	if (bb.SampleDesc.Count != 1 || !(bb.BindFlags & D3D11_BIND_RENDER_TARGET) || FAILED(g_device->CreateRenderTargetView(backBuffer, nullptr, &rtv)))
	{
		Log("Crosshair: the back buffer can't be drawn to (samples %u, bind flags %x)", bb.SampleDesc.Count, bb.BindFlags);
		g_crosshairBroken = true;
		backBuffer->Release();
		return;
	}
	static bool logged;
	if (!logged)
	{
		logged = true;
		Log("Crosshair: drawn at the headset's resolution");
	}

	ID3DDeviceContextState* previous = nullptr;
	g_context1->SwapDeviceContextState(g_hudState, &previous);
	g_context1->OMSetRenderTargets(1, &rtv, nullptr);
	g_context1->OMSetBlendState(g_crosshairBlend, nullptr, 0xffffffff);
	g_context1->RSSetState(g_crosshairRaster);
	g_context1->IASetInputLayout(nullptr);
	g_context1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	g_context1->VSSetShader(g_crosshairVs, nullptr, 0);
	g_context1->PSSetShader(g_crosshairPs, nullptr, 0);
	g_context1->PSSetSamplers(0, 1, &g_crosshairSampler);
	g_context1->PSSetShaderResources(0, 1, &g_crosshairSrv);
	g_context1->PSSetConstantBuffers(0, 1, &g_crosshairParams);
	float bw = (float)bb.Width, bh = (float)bb.Height;
	for (int eye = 0; eye < 2; ++eye)
	{
		const Avp2XrCrosshair& c = xh[eye];
		if (!c.visible || c.halfSize[0] <= 0.0f || c.halfSize[1] <= 0.0f)
			continue;
		D3D11_VIEWPORT vp = { (c.center[0] - c.halfSize[0]) * bw, (c.center[1] - c.halfSize[1]) * bh,
			c.halfSize[0] * 2.0f * bw, c.halfSize[1] * 2.0f * bh, 0.0f, 1.0f };
		const float* r = g_eyeSubmit[eye].rect;
		D3D11_RECT eyeRect = { (LONG)(r[0] * bw + 0.5f), (LONG)(r[1] * bh + 0.5f), (LONG)((r[0] + r[2]) * bw + 0.5f), (LONG)((r[1] + r[3]) * bh + 0.5f) };
		float params[4] = { c.alpha < 0.0f ? 0.0f : c.alpha > 1.0f ? 1.0f : c.alpha, 0.0f, 0.0f, 0.0f };
		g_context1->UpdateSubresource(g_crosshairParams, 0, nullptr, params, 0, 0);
		g_context1->RSSetViewports(1, &vp);
		g_context1->RSSetScissorRects(1, &eyeRect);
		g_context1->Draw(4, 0);
	}
	ID3D11ShaderResourceView* noSrv = nullptr;
	g_context1->PSSetShaderResources(0, 1, &noSrv);
	g_context1->OMSetBlendState(nullptr, nullptr, 0xffffffff);  // the HUD capture and mirror share this state
	g_context1->RSSetState(nullptr);
	g_context1->OMSetRenderTargets(0, nullptr, nullptr);
	g_context1->SwapDeviceContextState(previous, nullptr);
	SafeRelease(previous);
	rtv->Release();
	backBuffer->Release();
}

// Draws the markers the game gave for this frame over the eyes in the back buffer (under the crosshair)
static void DrawMarkers(IDXGISwapChain* sc)
{
	int count = g_markerCount;
	g_markerCount = 0;
	if (!count || g_markerBroken || g_crosshairBroken)
		return;
	if (((!g_hudState || !g_context1) && !CreateHudPipeline()) || (!g_crosshairVs && !CreateCrosshairPipeline()) ||
		(!g_markerVs && !CreateMarkerPipeline()))
	{
		Log("Markers: can't be drawn");
		SafeRelease(g_markerParams);
		SafeRelease(g_markerPs);
		SafeRelease(g_markerVs);
		g_markerBroken = true;
		return;
	}

	ID3D11Texture2D* backBuffer = nullptr;
	if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
		return;
	D3D11_TEXTURE2D_DESC bb;
	backBuffer->GetDesc(&bb);
	ID3D11RenderTargetView* rtv = nullptr;
	if (bb.SampleDesc.Count != 1 || !(bb.BindFlags & D3D11_BIND_RENDER_TARGET) || FAILED(g_device->CreateRenderTargetView(backBuffer, nullptr, &rtv)))
	{
		Log("Markers: the back buffer can't be drawn to (samples %u, bind flags %x)", bb.SampleDesc.Count, bb.BindFlags);
		g_markerBroken = true;
		backBuffer->Release();
		return;
	}
	static bool logged;
	if (!logged)
	{
		logged = true;
		Log("Markers: drawn at the headset's resolution");
	}

	ID3DDeviceContextState* previous = nullptr;
	g_context1->SwapDeviceContextState(g_hudState, &previous);
	g_context1->OMSetRenderTargets(1, &rtv, nullptr);
	g_context1->OMSetBlendState(g_crosshairBlend, nullptr, 0xffffffff);
	g_context1->RSSetState(g_crosshairRaster);
	g_context1->IASetInputLayout(nullptr);
	g_context1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	g_context1->VSSetShader(g_markerVs, nullptr, 0);
	g_context1->PSSetShader(g_markerPs, nullptr, 0);
	g_context1->PSSetSamplers(0, 1, &g_crosshairSampler);
	g_context1->VSSetConstantBuffers(0, 1, &g_markerParams);
	g_context1->PSSetConstantBuffers(0, 1, &g_markerParams);
	float bw = (float)bb.Width, bh = (float)bb.Height;
	D3D11_VIEWPORT vp = { 0.0f, 0.0f, bw, bh, 0.0f, 1.0f };
	g_context1->RSSetViewports(1, &vp);
	for (int i = 0; i < count; ++i)
	{
		const Avp2XrMarker& m = g_markers[i];
		if (m.eye < 0 || m.eye > 1 || m.image < 0 || m.image >= AVP2XR_MAX_MARKER_IMAGES ||
			m.halfSize[0] <= 0.0f || m.halfSize[1] <= 0.0f || !UpdateMarkerTexture(m.image))
			continue;
		// The corners turned in back buffer pixels (near enough square in the headset), then to clip space
		float cx = m.center[0] * bw, cy = m.center[1] * bh, hx = m.halfSize[0] * bw, hy = m.halfSize[1] * bh;
		float cs = cosf(m.angle), sn = sinf(m.angle);
		float params[12] = { m.alpha < 0.0f ? 0.0f : m.alpha > 1.0f ? 1.0f : m.alpha, 0.0f, 0.0f, 0.0f };
		for (int k = 0; k < 4; ++k)
		{
			float x = (k & 1) ? hx : -hx, y = (k >> 1) ? hy : -hy;  // top-left, top-right, bottom-left, bottom-right
			float px = cx + x * cs - y * sn, py = cy + x * sn + y * cs;
			params[4 + k * 2] = px / bw * 2.0f - 1.0f;
			params[5 + k * 2] = 1.0f - py / bh * 2.0f;
		}
		const float* r = g_eyeSubmit[m.eye].rect;
		D3D11_RECT eyeRect = { (LONG)(r[0] * bw + 0.5f), (LONG)(r[1] * bh + 0.5f), (LONG)((r[0] + r[2]) * bw + 0.5f), (LONG)((r[1] + r[3]) * bh + 0.5f) };
		g_context1->UpdateSubresource(g_markerParams, 0, nullptr, params, 0, 0);
		g_context1->RSSetScissorRects(1, &eyeRect);
		g_context1->PSSetShaderResources(0, 1, &g_markerImages[m.image].srv);
		g_context1->Draw(4, 0);
	}
	ID3D11ShaderResourceView* noSrv = nullptr;
	g_context1->PSSetShaderResources(0, 1, &noSrv);
	g_context1->OMSetBlendState(nullptr, nullptr, 0xffffffff);  // the HUD capture and mirror share this state
	g_context1->RSSetState(nullptr);
	g_context1->OMSetRenderTargets(0, nullptr, nullptr);
	g_context1->SwapDeviceContextState(previous, nullptr);
	SafeRelease(previous);
	rtv->Release();
	backBuffer->Release();
}

static XrCompositionLayerProjectionView g_projViews[2];

static void BuildProjection()
{
	for (int eye = 0; eye < 2; ++eye)
	{
		XrCompositionLayerProjectionView& pv = g_projViews[eye];
		pv = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
		pv.pose = g_views[eye].pose;
		pv.fov.angleLeft = g_eyeSubmit[eye].fov[0];
		pv.fov.angleRight = g_eyeSubmit[eye].fov[1];
		pv.fov.angleUp = g_eyeSubmit[eye].fov[2];
		pv.fov.angleDown = g_eyeSubmit[eye].fov[3];
		pv.subImage.swapchain = g_swapchain;
		pv.subImage.imageRect = EyeRectPixels(g_eyeSubmit[eye].rect);
	}
	g_projection.space = g_space;
	g_projection.viewCount = 2;
	g_projection.views = g_projViews;
}

static XrCompositionLayerQuad MakeQuad(XrSwapchain swapchain, UINT width, UINT height, float distance, float quadWidth, float y)
{
	XrCompositionLayerQuad quad = { XR_TYPE_COMPOSITION_LAYER_QUAD };
	quad.space = g_space;
	quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
	quad.subImage.swapchain = swapchain;
	quad.subImage.imageRect.extent.width = (int32_t)width;
	quad.subImage.imageRect.extent.height = (int32_t)height;
	quad.pose.orientation.w = 1.0f;
	quad.pose.position.y = y;
	quad.pose.position.z = -distance;
	quad.size.width = quadWidth;
	// Use the game's aspect ratio: with a forced dgVoodoo resolution the image is stretched.
	if (g_gameWidth > 0)
		quad.size.height = quadWidth * (float)g_gameHeight / (float)g_gameWidth;
	else
		quad.size.height = quadWidth * (float)height / (float)width;
	return quad;
}

static void LogOutputMode(OutputMode mode)
{
	static OutputMode last = OUTPUT_NONE;
	if (mode == last)
		return;
	last = mode;
	if (mode == OUTPUT_FLAT)
		Log("Output: flat screen");
	else
	{
		XrRect2Di l = g_projViews[0].subImage.imageRect, r = g_projViews[1].subImage.imageRect;
		Log("Output: stereo%s (left eye %d,%d %dx%d, right eye %d,%d %dx%d)", mode == OUTPUT_STEREO_HUD ? " + HUD" : "",
			l.offset.x, l.offset.y, l.extent.width, l.extent.height, r.offset.x, r.offset.y, r.extent.width, r.extent.height);
	}
}

static void EndXrFrame(const XrCompositionLayerBaseHeader* const* layers, uint32_t layerCount)
{
	XrFrameEndInfo fei = { XR_TYPE_FRAME_END_INFO };
	fei.displayTime = g_frameState.predictedDisplayTime;
	fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	fei.layerCount = layerCount;
	fei.layers = layers;
	XrResult r = x_xrEndFrame(g_session, &fei);
	if (XR_FAILED(r))
		Log("xrEndFrame failed: %s", XrStr(r));
	g_frameBegun = false;
	g_stereoSubmitted = g_viewsValid = g_hudFollows = false;
	memset(g_crosshairs, 0, sizeof(g_crosshairs));
}

static PresentAction XrFrame(IDXGISwapChain* sc)
{
	if (g_hudNext)
	{
		// The second flip of a stereo frame: the HUD alone. Finish the frame begun for the eyes.
		g_hudNext = false;
		if (!g_frameBegun)
		{
			g_mirrorPending = false;
			return PRESENT_SKIP;  // the eye capture failed and that frame already ended
		}
		const XrCompositionLayerBaseHeader* layers[2] = { (const XrCompositionLayerBaseHeader*)&g_projection };
		uint32_t layerCount = 1;
		XrCompositionLayerQuad hud;
		double t0 = NowMs();
		bool hudOk = CapturePanel(g_hud, sc);
		g_timeHud += NowMs() - t0;
		if (hudOk)
		{
			hud = MakeQuad(g_hud.swapchain, g_hud.width, g_hud.height, g_cfg.hudDistance, g_cfg.hudWidth, g_cfg.hudHeight);
			hud.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
			hud.space = g_viewSpace;  // head-locked
			layers[layerCount++] = (const XrCompositionLayerBaseHeader*)&hud;
		}
		t0 = NowMs();
		EndXrFrame(layers, layerCount);
		g_timeEnd += NowMs() - t0;

		// The monitor mirror: the kept eye image with this HUD over it
		PresentAction hudAction = PRESENT_SKIP;
		if (g_mirrorPending)
		{
			g_mirrorPending = false;
			t0 = NowMs();
			if (MirrorDraw(sc, g_cfg.mirrorEye, hudOk ? g_hud.srv : nullptr))
			{
				hudAction = PRESENT_NO_VSYNC;
				g_presentIsEye = true;
			}
			g_timePresent += NowMs() - t0;
		}

		LogTiming();
		LogOutputMode(layerCount == 2 ? OUTPUT_STEREO_HUD : OUTPUT_STEREO);
		return hudAction;
	}

	if (!BeginXrFrame())
	{
		MirrorFlat(sc);  // no XR frame, but the monitor still gets the game's aspect ratio
		return PRESENT_AS_IS;
	}

	// Stereo: the eye images go to the headset as they are. Flat: the game's screen is made at the
	// game's resolution (falling back to the stretched back buffer if that can't be set up).
	double t0 = NowMs();
	bool screenPanel = false;
	bool copied = false;
	if (g_frameState.shouldRender)
	{
		if (!g_stereoSubmitted)
			copied = screenPanel = CapturePanel(g_screen, sc);
		else
		{
			ApplyGamma(sc);
			DrawMarkers(sc);
			DrawCrosshairs(sc);
		}
		if (!copied)
			copied = CopyFrame(sc);
	}
	if (copied && g_stereoSubmitted)
	{
		g_timeCopy += NowMs() - t0;
		BuildProjection();

		// The eye image on the monitor too? It costs a full-size present every frame.
		static unsigned mirrorCount;
		PresentAction eyeAction = PRESENT_SKIP;
		if (g_cfg.desktopMirror > 0 && ++mirrorCount >= (unsigned)g_cfg.desktopMirror)
		{
			mirrorCount = 0;

			// Others watching the monitor get one eye filling the screen, not both side by side,
			// with the HUD over it: when the HUD image follows, that flip presents the mirror.
			double tm = NowMs();
			if (g_cfg.mirrorEye != 2 && MirrorKeepEyes(sc))
			{
				if (g_hudFollows)
					g_mirrorPending = true;
				else if (MirrorDraw(sc, g_cfg.mirrorEye, nullptr))
				{
					eyeAction = PRESENT_NO_VSYNC;
					g_presentIsEye = true;
				}
			}
			else
			{
				eyeAction = PRESENT_NO_VSYNC;
				g_presentIsEye = true;
			}
			g_timePresent += NowMs() - tm;
		}

		if (g_hudFollows)
		{
			// Keep the frame open until the HUD image arrives with the next Present.
			g_hudNext = true;
			return eyeAction;
		}
		const XrCompositionLayerBaseHeader* layers[1] = { (const XrCompositionLayerBaseHeader*)&g_projection };
		EndXrFrame(layers, 1);
		LogTiming();
		LogOutputMode(OUTPUT_STEREO);
		return eyeAction;
	}

	if (g_stereoSubmitted && g_hudFollows)
		g_hudNext = true;  // drop the HUD-only image that follows this failed frame
	XrCompositionLayerQuad quad;
	const XrCompositionLayerBaseHeader* layers[1] = { (const XrCompositionLayerBaseHeader*)&quad };
	if (copied)
		quad = screenPanel ? MakeQuad(g_screen.swapchain, g_screen.width, g_screen.height, g_cfg.distance, g_cfg.width, g_cfg.height)
			: MakeQuad(g_swapchain, g_scWidth, g_scHeight, g_cfg.distance, g_cfg.width, g_cfg.height);
	EndXrFrame(layers, copied ? 1 : 0);
	if (copied)
		LogOutputMode(OUTPUT_FLAT);
	MirrorFlat(sc);  // the monitor gets the game's aspect ratio, not dgVoodoo's stretch
	return PRESENT_NO_VSYNC;
}

// MirrorFillScreen: a windowed game's window (its client area is the game's resolution, so the
// monitor showed it small) is stretched borderless over its monitor. Checked twice a second; if
// something keeps putting it back, it's left alone after a few tries.
// FocusGameWindow: for the first 10 seconds after the game's window first presents, until it has
// been in front once, it's brought to the front if it isn't. Windows only lets a program take the
// foreground in some cases, so: a plain request, then with this thread's input joined to the
// current foreground window's, then (once) after a tap of Alt, which counts as input.
static void FocusGameWindowAtStart(HWND wnd)
{
	static HWND focusWindow;
	static DWORD startTime, nextTry;
	static bool done, altTried;
	if (!g_cfg.focusWindow || !wnd || done)
		return;
	DWORD now = GetTickCount();
	if (wnd != focusWindow)
	{
		focusWindow = wnd;
		startTime = now;
		nextTry = now;
	}
	if (GetForegroundWindow() == wnd)
	{
		done = true;
		Log("Window: the game's window is in front");
		return;
	}
	if (now - startTime > 10000)
	{
		done = true;
		Log("Window: couldn't bring the game's window to the front (alt-tab to it for the keyboard)");
		return;
	}
	if ((int)(now - nextTry) < 0 || !IsWindowVisible(wnd) || IsIconic(wnd))
		return;
	nextTry = now + 500;

	const char* how = "asked";
	SetForegroundWindow(wnd);
	if (GetForegroundWindow() != wnd)
	{
		HWND fg = GetForegroundWindow();
		DWORD fgThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0, me = GetCurrentThreadId();
		if (fgThread && fgThread != me && AttachThreadInput(me, fgThread, TRUE))
		{
			BringWindowToTop(wnd);
			SetForegroundWindow(wnd);
			AttachThreadInput(me, fgThread, FALSE);
			how = "input joined";
		}
	}
	if (GetForegroundWindow() != wnd && !altTried)
	{
		altTried = true;
		keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY, 0);
		SetForegroundWindow(wnd);
		keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
		how = "alt tapped";
	}
	if (GetForegroundWindow() == wnd)
	{
		done = true;
		Log("Window: brought the game's window to the front (%s)", how);
	}
}

static void FillMonitorIfWindowed(const DXGI_SWAP_CHAIN_DESC& scd)
{
	static HWND lastWindow;
	static int tries;
	static DWORD nextCheck;
	if (!g_cfg.mirrorFill || !scd.Windowed || !scd.OutputWindow)
		return;
	if (scd.OutputWindow != lastWindow)
	{
		lastWindow = scd.OutputWindow;
		tries = 0;
	}
	DWORD now = GetTickCount();
	if ((int)(now - nextCheck) < 0 || tries >= 5)
		return;
	nextCheck = now + 500;

	HWND wnd = scd.OutputWindow;
	if (!IsWindowVisible(wnd) || IsIconic(wnd))
		return;
	MONITORINFO mi = { sizeof(mi) };
	if (!GetMonitorInfoW(MonitorFromWindow(wnd, MONITOR_DEFAULTTOPRIMARY), &mi))
		return;
	RECT r;
	GetWindowRect(wnd, &r);
	if (EqualRect(&r, &mi.rcMonitor))
		return;

	LONG style = GetWindowLongW(wnd, GWL_STYLE);
	style &= ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER | WS_DLGFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
	SetWindowLongW(wnd, GWL_STYLE, style | WS_POPUP);
	SetWindowPos(wnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
		mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
	++tries;
	Log("Mirror: the game is windowed (%ldx%ld); its window now fills the monitor (%ldx%ld)%s", r.right - r.left, r.bottom - r.top,
		mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
		tries >= 5 ? ", and won't be resized again (something keeps changing it)" : "");
}

static PresentAction OnPresent(IDXGISwapChain* sc, UINT flags)
{
	if (flags & DXGI_PRESENT_TEST)
		return PRESENT_AS_IS;

	DXGI_SWAP_CHAIN_DESC scd;
	static UINT lastWidth, lastHeight;
	static BOOL lastWindowed = -1;
	if (SUCCEEDED(sc->GetDesc(&scd)))
	{
		if (scd.BufferDesc.Width != lastWidth || scd.BufferDesc.Height != lastHeight || scd.Windowed != lastWindowed)
		{
			lastWidth = scd.BufferDesc.Width;
			lastHeight = scd.BufferDesc.Height;
			lastWindowed = scd.Windowed;
			Log("Back buffer %ux%u, format %d, %s", lastWidth, lastHeight, scd.BufferDesc.Format, scd.Windowed ? "windowed" : "fullscreen");
		}
		FillMonitorIfWindowed(scd);
		FocusGameWindowAtStart(scd.OutputWindow);
	}

	if (g_xrState == XRS_DISABLED)
		return PRESENT_AS_IS;

	ID3D11Device* device = nullptr;
	if (FAILED(sc->GetDevice(IID_PPV_ARGS(&device))))
		return PRESENT_AS_IS;  // not a D3D11 swapchain

	if (g_xrState == XRS_ACTIVE && device != g_device)
	{
		Log("dgVoodoo switched to a new D3D11 device; restarting the OpenXR session");
		XrShutdown(false);
	}
	if (g_xrState == XRS_UNINIT && (int)(GetTickCount() - g_retryAt) >= 0)
	{
		if (!g_waitingForHmd)
			Log("Starting OpenXR on device %p", device);
		if (XrInit(device))
			g_xrState = XRS_ACTIVE;
		else if (g_waitingForHmd)
		{
			XrShutdown(false);
			g_retryAt = GetTickCount() + 5000;
		}
		else
		{
			Log("OpenXR init failed; the game will run on the desktop only");
			XrShutdown(true);
		}
	}
	device->Release();

	if (g_xrState != XRS_ACTIVE)
		return PRESENT_AS_IS;
	return XrFrame(sc);
}

// ---------------------------------------------------------------------------------------------
// Present hooks

typedef HRESULT(STDMETHODCALLTYPE* PFN_Present)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_Present1)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
static PFN_Present g_realPresent;
static PFN_Present1 g_realPresent1;
static int t_inPresent;  // DXGI may route Present through Present1

static HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* sc, UINT syncInterval, UINT flags)
{
	PresentAction action = t_inPresent++ == 0 ? OnPresent(sc, flags) : PRESENT_AS_IS;
	HRESULT hr = S_OK;
	if (action != PRESENT_SKIP)
	{
		double t0 = g_presentIsEye ? NowMs() : 0.0;
		hr = g_realPresent(sc, action == PRESENT_NO_VSYNC ? 0 : syncInterval, flags);
		if (g_presentIsEye)
			g_timePresent += NowMs() - t0;
	}
	g_presentIsEye = false;
	--t_inPresent;
	return hr;
}

static HRESULT STDMETHODCALLTYPE HookPresent1(IDXGISwapChain1* sc, UINT syncInterval, UINT flags, const DXGI_PRESENT_PARAMETERS* params)
{
	PresentAction action = t_inPresent++ == 0 ? OnPresent(sc, flags) : PRESENT_AS_IS;
	HRESULT hr = S_OK;
	if (action != PRESENT_SKIP)
	{
		double t0 = g_presentIsEye ? NowMs() : 0.0;
		hr = g_realPresent1(sc, action == PRESENT_NO_VSYNC ? 0 : syncInterval, flags, params);
		if (g_presentIsEye)
			g_timePresent += NowMs() - t0;
	}
	g_presentIsEye = false;
	--t_inPresent;
	return hr;
}

static void PatchVtable(void** vtable, int slot, void* hook, void** original)
{
	if (vtable[slot] == hook)
		return;
	DWORD old;
	VirtualProtect(&vtable[slot], sizeof(void*), PAGE_READWRITE, &old);
	*original = vtable[slot];
	vtable[slot] = hook;
	VirtualProtect(&vtable[slot], sizeof(void*), old, &old);
}

// All DXGI swapchains share one vtable inside dxgi.dll. Making a throwaway swapchain on a
// hidden window gets us that vtable, and patching it catches dgVoodoo's swapchains too.
static void InstallPresentHook()
{
	WNDCLASSW wc = {};
	wc.lpfnWndProc = DefWindowProcW;
	wc.hInstance = g_hModule;
	wc.lpszClassName = L"avp2xr_dummy";
	RegisterClassW(&wc);
	HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, g_hModule, nullptr);

	DXGI_SWAP_CHAIN_DESC scd = {};
	scd.BufferCount = 1;
	scd.BufferDesc.Width = 64;
	scd.BufferDesc.Height = 64;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.OutputWindow = hwnd;
	scd.SampleDesc.Count = 1;
	scd.Windowed = TRUE;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	IDXGISwapChain* sc = nullptr;
	ID3D11Device* dev = nullptr;
	HRESULT hr = g_realCreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
		D3D11_SDK_VERSION, &scd, &sc, &dev, nullptr, nullptr);
	if (FAILED(hr))
	{
		Log("Dummy swapchain creation failed (0x%08lx); Present hook not installed", hr);
	}
	else
	{
		void** vtable = *(void***)sc;
		PatchVtable(vtable, 8, (void*)HookPresent, (void**)&g_realPresent);
		IDXGISwapChain1* sc1 = nullptr;
		if (SUCCEEDED(sc->QueryInterface(IID_PPV_ARGS(&sc1))))
		{
			PatchVtable(*(void***)sc1, 22, (void*)HookPresent1, (void**)&g_realPresent1);
			sc1->Release();
		}
		Log("Present hook installed (Present1 %s)", g_realPresent1 ? "hooked too" : "not available");
		sc->Release();
		dev->Release();
	}
	DestroyWindow(hwnd);
	UnregisterClassW(wc.lpszClassName, g_hModule);
}

static void OnDeviceCreated(HRESULT hr, ID3D11Device** device)
{
	static LONG hooked;
	if (FAILED(hr) || !device || !*device)
		return;
	Log("D3D11 device created: %p, feature level 0x%x", *device, (*device)->GetFeatureLevel());
	if (InterlockedExchange(&hooked, 1) == 0)
	{
		// dgVoodoo frees d3d11.dll after probing it, but the patched DXGI vtable outlives
		// that, so this module must stay loaded for the rest of the process.
		HMODULE self;
		if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
				(LPCWSTR)&OnDeviceCreated, &self))
		{
			Log("Could not pin avp2xr in memory (error %lu); Present hook not installed", GetLastError());
			return;
		}
		InstallPresentHook();
	}
}

// Some XR runtimes use the immediate context from their own threads, which a single-threaded
// device doesn't allow.
static UINT FixFlags(UINT flags)
{
	if (flags & D3D11_CREATE_DEVICE_SINGLETHREADED)
		Log("Removing D3D11_CREATE_DEVICE_SINGLETHREADED");
	return flags & ~D3D11_CREATE_DEVICE_SINGLETHREADED;
}

extern "C" HRESULT WINAPI Proxy_D3D11CreateDevice(IDXGIAdapter* adapter, D3D_DRIVER_TYPE driverType, HMODULE software,
	UINT flags, const D3D_FEATURE_LEVEL* featureLevels, UINT numFeatureLevels, UINT sdkVersion,
	ID3D11Device** device, D3D_FEATURE_LEVEL* featureLevel, ID3D11DeviceContext** context)
{
	HRESULT hr = g_realCreateDevice(adapter, driverType, software, FixFlags(flags), featureLevels, numFeatureLevels,
		sdkVersion, device, featureLevel, context);
	OnDeviceCreated(hr, device);
	return hr;
}

extern "C" HRESULT WINAPI Proxy_D3D11CreateDeviceAndSwapChain(IDXGIAdapter* adapter, D3D_DRIVER_TYPE driverType,
	HMODULE software, UINT flags, const D3D_FEATURE_LEVEL* featureLevels, UINT numFeatureLevels, UINT sdkVersion,
	const DXGI_SWAP_CHAIN_DESC* swapChainDesc, IDXGISwapChain** swapChain, ID3D11Device** device,
	D3D_FEATURE_LEVEL* featureLevel, ID3D11DeviceContext** context)
{
	HRESULT hr = g_realCreateDeviceAndSwapChain(adapter, driverType, software, FixFlags(flags), featureLevels,
		numFeatureLevels, sdkVersion, swapChainDesc, swapChain, device, featureLevel, context);
	OnDeviceCreated(hr, device);
	return hr;
}

// ---------------------------------------------------------------------------------------------

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		g_hModule = hinst;
		InitializeCriticalSection(&g_logLock);
		GetModuleFileNameW(hinst, g_moduleDir, MAX_PATH);
		if (wchar_t* slash = wcsrchr(g_moduleDir, L'\\'))
			slash[1] = 0;

		wchar_t exe[MAX_PATH];
		GetModuleFileNameW(nullptr, exe, MAX_PATH);
		Log("avp2xr d3d11 proxy loaded into %ls", exe);
		if (!LoadRealD3D11())
		{
			Log("Could not load the system d3d11.dll");
			return FALSE;
		}
		LoadSettings();
	}
	else if (reason == DLL_PROCESS_DETACH)
		Log("avp2xr unloading (process exit: %s)", reserved ? "yes" : "no");
	return TRUE;
}
