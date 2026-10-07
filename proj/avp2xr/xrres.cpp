// xrres.exe: sets dgVoodoo's forced resolution (the size the game renders both eyes at) from the OpenXR
// runtime's recommended per-eye size times avp2xr.ini [VR] RenderScale, so it follows SteamVR's render
// resolution setting. The AVP2VR launchers run it from the game folder just before starting the game.
//
// The game folder is usually under Program Files, which only an administrator can write to. lithtech.exe
// has no manifest, so Windows virtualizes its file access: a copy of dgVoodoo.conf in
// %LOCALAPPDATA%\VirtualStore\<game folder> is what dgVoodoo (inside the game) reads. So this reads the
// game folder's dgVoodoo.conf (what Install_avp2vr.bat installed), changes [DirectX] Resolution and
// writes the result there; outside Program Files it changes the game folder's copy instead.
// On failure (no runtime, headset asleep) nothing is written and the previous resolution stays.
// "xrres.exe <width>x<height>" uses that per-eye size instead of asking the runtime (for testing).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <openxr/openxr.h>

static std::wstring g_dir;  // the folder this exe is in (the game folder), with a trailing backslash

static void Fail(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "xrres: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "; the render resolution is left as it was\n");
	va_end(args);
	exit(1);
}

static bool QueryRecommended(uint32_t& width, uint32_t& height, uint32_t& maxWidth, uint32_t& maxHeight)
{
	HMODULE loader = LoadLibraryW((g_dir + L"openxr_loader.dll").c_str());
	if (!loader)
		Fail("could not load openxr_loader.dll (error %lu)", GetLastError());
	PFN_xrGetInstanceProcAddr getProc = (PFN_xrGetInstanceProcAddr)GetProcAddress(loader, "xrGetInstanceProcAddr");
	if (!getProc)
		Fail("openxr_loader.dll has no xrGetInstanceProcAddr");
	PFN_xrCreateInstance createInstance = nullptr;
	getProc(XR_NULL_HANDLE, "xrCreateInstance", (PFN_xrVoidFunction*)&createInstance);
	if (!createInstance)
		Fail("no xrCreateInstance");

	XrInstanceCreateInfo ci = { XR_TYPE_INSTANCE_CREATE_INFO };
	strcpy_s(ci.applicationInfo.applicationName, "AVP2VR resolution check");
	ci.applicationInfo.applicationVersion = 1;
	strcpy_s(ci.applicationInfo.engineName, "LithTech");
	ci.applicationInfo.engineVersion = 1;
	ci.applicationInfo.apiVersion = XR_API_VERSION_1_0;
	XrInstance instance = XR_NULL_HANDLE;
	XrResult r = createInstance(&ci, &instance);
	if (XR_FAILED(r))
		Fail("xrCreateInstance failed (%d): is the OpenXR runtime (SteamVR / Meta) available?", (int)r);

	PFN_xrGetSystem getSystem = nullptr;
	PFN_xrEnumerateViewConfigurationViews enumViews = nullptr;
	PFN_xrDestroyInstance destroyInstance = nullptr;
	getProc(instance, "xrGetSystem", (PFN_xrVoidFunction*)&getSystem);
	getProc(instance, "xrEnumerateViewConfigurationViews", (PFN_xrVoidFunction*)&enumViews);
	getProc(instance, "xrDestroyInstance", (PFN_xrVoidFunction*)&destroyInstance);
	if (!getSystem || !enumViews || !destroyInstance)
		Fail("the OpenXR runtime is missing functions");

	XrSystemGetInfo sgi = { XR_TYPE_SYSTEM_GET_INFO };
	sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	XrSystemId system = XR_NULL_SYSTEM_ID;
	r = getSystem(instance, &sgi, &system);
	bool ok = false;
	if (XR_FAILED(r))
		fprintf(stderr, "xrres: no headset (%d): is it connected and awake?\n", (int)r);
	else
	{
		XrViewConfigurationView vcv[2] = { { XR_TYPE_VIEW_CONFIGURATION_VIEW }, { XR_TYPE_VIEW_CONFIGURATION_VIEW } };
		uint32_t count = 0;
		r = enumViews(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &count, vcv);
		if (XR_SUCCEEDED(r) && count == 2)
		{
			width = vcv[0].recommendedImageRectWidth;
			height = vcv[0].recommendedImageRectHeight;
			maxWidth = vcv[0].maxImageRectWidth;
			maxHeight = vcv[0].maxImageRectHeight;
			ok = width && height;
		}
		else
			fprintf(stderr, "xrres: xrEnumerateViewConfigurationViews failed (%d)\n", (int)r);
	}
	destroyInstance(instance);
	return ok;
}

static double ReadRenderScale()
{
	wchar_t buf[64];
	GetPrivateProfileStringW(L"VR", L"RenderScale", L"", buf, 64, (g_dir + L"avp2xr.ini").c_str());
	double scale = buf[0] ? _wtof(buf) : 1.0;
	if (scale < 0.3 || scale > 2.0)
	{
		printf("xrres: RenderScale=%ls is outside 0.3 to 2; using 1\n", buf);
		scale = 1.0;
	}
	return scale;
}

static bool ReadText(const std::wstring& path, std::string& text)
{
	FILE* f = nullptr;
	if (_wfopen_s(&f, path.c_str(), L"rb") || !f)
		return false;
	char chunk[4096];
	size_t n;
	while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0)
		text.append(chunk, n);
	fclose(f);
	return true;
}

// Replaces the value of "Resolution = ..." in the [DirectX] section, keeping the line's layout.
static bool SetDirectXResolution(std::string& text, const std::string& value)
{
	bool inDirectX = false;
	size_t pos = 0;
	while (pos < text.size())
	{
		size_t end = text.find('\n', pos);
		if (end == std::string::npos)
			end = text.size();
		size_t start = text.find_first_not_of(" \t", pos);
		if (start < end && text[start] == '[')
			inDirectX = _strnicmp(&text[start], "[DirectX]", 9) == 0;
		else if (inDirectX && start < end && _strnicmp(&text[start], "Resolution", 10) == 0)
		{
			size_t eq = text.find('=', start);
			if (eq < end)
			{
				size_t valueEnd = end;
				if (valueEnd > eq && text[valueEnd - 1] == '\r')
					--valueEnd;
				text.replace(eq + 1, valueEnd - eq - 1, " " + value);
				return true;
			}
		}
		pos = end + 1;
	}
	return false;
}

// Where the game reads dgVoodoo.conf from: the VirtualStore copy when the game folder is in a
// virtualized location (Program Files, Windows), else the game folder's own.
static std::wstring TargetPath()
{
	std::wstring conf = g_dir + L"dgVoodoo.conf";
	const wchar_t* roots[] = { L"ProgramFiles", L"ProgramFiles(x86)", L"SystemRoot" };
	for (const wchar_t* var : roots)
	{
		wchar_t root[MAX_PATH];
		DWORD n = GetEnvironmentVariableW(var, root, MAX_PATH);
		if (!n || n >= MAX_PATH)
			continue;
		std::wstring r = root;
		if (r.back() != L'\\')
			r += L'\\';
		if (_wcsnicmp(conf.c_str(), r.c_str(), r.size()) != 0)
			continue;
		wchar_t local[MAX_PATH];
		n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
		if (!n || n >= MAX_PATH || conf.size() < 3 || conf[1] != L':')
			break;
		return std::wstring(local) + L"\\VirtualStore" + conf.substr(2);  // drop the drive letter
	}
	return conf;
}

static bool CreateFolders(const std::wstring& file)
{
	for (size_t p = file.find(L'\\', 3); p != std::wstring::npos; p = file.find(L'\\', p + 1))
	{
		std::wstring dir = file.substr(0, p);
		if (!CreateDirectoryW(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
			return false;
	}
	return true;
}

int main(int argc, char** argv)
{
	wchar_t exe[MAX_PATH];
	GetModuleFileNameW(nullptr, exe, MAX_PATH);
	g_dir = exe;
	g_dir.resize(g_dir.find_last_of(L'\\') + 1);

	uint32_t recW = 0, recH = 0, maxW = 0, maxH = 0;
	if (argc > 1)
	{
		if (sscanf_s(argv[1], "%ux%u", &recW, &recH) != 2 || !recW || !recH)
			Fail("expected a per-eye size such as 2244x2352, not %s", argv[1]);
	}
	else if (!QueryRecommended(recW, recH, maxW, maxH))
		Fail("could not get the headset's recommended resolution");

	// Per eye, scaled on each axis and rounded to even numbers; the back buffer holds both eyes side by side
	double scale = ReadRenderScale();
	uint32_t eyeW = (uint32_t)(recW * scale / 2 + 0.5) * 2;
	uint32_t eyeH = (uint32_t)(recH * scale / 2 + 0.5) * 2;
	if (maxW && eyeW > maxW) eyeW = maxW & ~1u;
	if (maxH && eyeH > maxH) eyeH = maxH & ~1u;
	char value[64];
	sprintf_s(value, "h:%u, v:%u", eyeW * 2, eyeH);

	std::string text;
	if (!ReadText(g_dir + L"dgVoodoo.conf", text))
		Fail("could not read %ls", (g_dir + L"dgVoodoo.conf").c_str());
	if (!SetDirectXResolution(text, value))
		Fail("dgVoodoo.conf has no Resolution line in [DirectX]");

	std::wstring target = TargetPath();
	FILE* f = nullptr;
	if (!CreateFolders(target) || _wfopen_s(&f, target.c_str(), L"wb") || !f)
		Fail("could not write %ls", target.c_str());
	bool written = fwrite(text.data(), 1, text.size(), f) == text.size();
	written = fclose(f) == 0 && written;
	if (!written)
		Fail("could not write %ls", target.c_str());

	printf("Render resolution %ux%u per eye (%s %ux%u x RenderScale %.2f): dgVoodoo Resolution = %s in %ls\n",
		eyeW, eyeH, argc > 1 ? "given" : "recommended", recW, recH, scale, value, target.c_str());
	return 0;
}
