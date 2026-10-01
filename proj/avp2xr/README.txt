avp2xr - dgVoodoo2 + D3D11 hook + OpenXR for AVP2 (32-bit)

How it fits together
  AVP2 (d3d.ren, Direct3D 7) -> dgVoodoo2 DDraw.dll/D3DImm.dll -> d3d11.dll (this proxy) -> system d3d11.dll
  The proxy forwards all d3d11 exports to SysWOW64\d3d11.dll, hooks IDXGISwapChain::Present/Present1,
  and on the first Present creates an OpenXR session (XR_KHR_D3D11_enable) on dgVoodoo's device.
  Each frame's back buffer is copied into an OpenXR swapchain and shown as a flat quad
  (see avp2xr.ini). This is not stereo 3D.

Files
  avp2xr.cpp / d3d11.def   proxy source; build.bat builds bin\d3d11.dll (Win32, VS2022)
  test\xrtest.cpp          smoke test: device + separate DXGI swapchain, 60 presents
  dgVoodoo.conf            stock 2.87.5 config with OutputAPI = d3d11_fl11_0 (D3D12 output would bypass
                           the hook), dgVoodooWatermark = false, and [DirectX] Resolution = h:4488, v:2352
  deploy\                  everything that goes into the game folder
  install.bat/uninstall.bat  copy to / remove from the game folder (run as administrator); install.bat also
                           installs deploy\avp2xr.ini (the settings file to edit; the old one is kept as .bak)
                           and deploy\vrrez\cshell.dll (see "Loading the VR cshell" below)
  "AVP2VR_(SteamVR_runtime).bat"  launches the game in VR with SteamVR's 32-bit OpenXR runtime (found through
                           openvrpaths.vrpath or Steam's registry key), whichever runtime is the system's active
                           one; SteamVR starts by itself. Use this with Meta Quest Link as well (Link + SteamVR).
  "AVP2VR_(Meta_runtime).bat"     the same, with XR_RUNTIME_JSON pointing at the Meta 32-bit runtime. Link 1.208's
                           32-bit runtime crashes all 32-bit OpenXR programs in xrCreateSession (in Meta's
                           RuntimeIPCServiceClient_32.dll; the 64-bit one works), so this crashes until Meta fixes it.
  "AVP2VR_(stereo_off_test).bat"  the VR cshell with VREnable 0 (flat virtual screen), for diagnosis
  The launchers can be added to Steam as a non-Steam game (set as VR) and started from SteamVR:
  Steam's 64-bit XR_RUNTIME_JSON is cleared by the launcher (and ignored by the proxy if still set).
  The launchers also pass avp2xr.ini's GameResolution (e.g. 640x480x32) on the command line, which
  overrides the display mode the original AVP2.exe launcher saves in autoexec.cfg.

Third-party (K:\Coding\Aliens-Versus-Predator-2-VR\proj\thirdparty): dgVoodoo2 v2.87.5, OpenXR loader/headers release-1.1.63.

Log: %LOCALAPPDATA%\avp2xr\avp2xr.log (not the game folder, which Windows redirects to VirtualStore).

Stereo (option 2)
  cshell.dll (K:\Coding\Aliens-Versus-Predator-2-VR\proj\AVP2\ClientShellDLL: VRMgr.cpp, CameraMgr::RenderStereo) calls avp2xr_GetApi() from the proxy
  (see avp2xr_api.h). While playing, each fullscreen camera is rendered once per eye into the left/right half
  of the back buffer, and the proxy submits the halves as an OpenXR projection layer. Menus, loading screens,
  or a headset that isn't tracking fall back to the flat screen.
  Console: VREnable 0/1, VRWorldScale <units per metre, default 55>.
  View: the player's view turns only with the head. It is anchored to the player's facing on the first stereo
  frame after any flat period (level load, menus). Cutscene cameras keep their scripted rotation with the head
  on top.
  Controllers (API v4, GetInput): left thumbstick moves relative to where the head faces (up forward, down
  back, left/right strafe; analog speed, 0.2 dead zone). The player's body (yaw and aim pitch) follows the
  head, so the motion tracker, flashlight etc. match where you look. Shots come from the right controller
  (CWeaponModel::GetFireInfo uses VRMgr::GetAimPose), and the first-person weapon is drawn there
  (VRMgr::PlaceWeaponForEye, per eye). The weapon's crosshair is drawn in each eye where the aim hits (right depth; the HUD copy is left out; Crosshair toggles it). Without tracked controllers the mouse/keyboard work as
  before. Bindings: Oculus Touch, Index, WMR, Vive (trackpad), khr/simple (aim only).
  Long presses: "long <input>" in a binding fires after LongPressTime (avp2xr.ini, default 0.5 s); such inputs
  are read through hidden longpressN actions and a quick press gives their other actions on release.
  Bindings are in avp2xr.ini, one section per controller type ([Touch] for Quest/Rift, [Index], [WMR],
  [Vive], [Simple]), e.g. Jump=right a; a refused section falls back to the defaults (logged). Taunt
  (AVP2XR_BTN_TAUNT -> COMMAND_ID_TAUNT, only does anything in multiplayer) is unbound unless the ini binds it.
  Menus: the proxy reads the controllers every XR frame (flat too). Outside GS_PLAYING, VRMgr::UpdateMenus turns
  either stick into arrow keys (with repeat for sliders), MenuSelect (right A) into Enter and MenuBack (right B)
  or Pause (left menu) into Escape, through CGameClientShell::OnKeyDown/OnKeyUp. Pause in play opens the menu.
  Buttons (API v7, Touch; Index uses left A/B for X/Y): right trigger fire, right grip alt fire, A jump,
  B crouch, X use, Y vision mode, left grip reload; right stick up/down next/previous weapon (opens the
  weapon chooser; the highlighted weapon is selected 0.6 s after the last push, or at once with the trigger). Presses go to
  CGameClientShell::OnCommandOn/Off like keys (VRMgr::UpdateButtons); held fire/altfire/jump/crouch also set
  the control flags in PlayerMovement::Update.
  Weapon model (VR): stays a really-close view model, placed per eye at 1/VRGunScale of the distance to the
  right controller, which looks the same as full size there but keeps the game's view-model lighting
  (VRGunMode 0 = scaled-up world object instead; VRMgr::PlaceWeapon/PlaceWeaponForEye). The model's right palm goes on the right
  controller's grip pose (API v9, where OpenXR puts the palm) and it points along the aim pose; tune with VRGunScale, default 12
  because the first-person models are built about 1/12 size, VRGunOffsetX/Y/Z, and VRGripX/Y/Z for models
  without hand bones). Two-handed aiming (API v8 off-hand grip pose):
  with the left hand 15-75 cm ahead of the right and within 40 deg of its aim, the gun and shots aim from
  the right hand to the left (VRTwoHanded 0 turns it off).
  Turning: right stick left/right, avp2xr.ini TurnMode=smooth (SmoothTurnSpeed deg/s at full push, default)
  or snap (SnapTurnAngle degrees per push).
  Recenter: hold the Meta/system button (the runtime recenters; the proxy passes the LOCAL space change on as
  Avp2XrInput::recenterCount and the game re-anchors: view faces the aim, eyes back at the camera height).
  Tests: test\stereotest.cpp (proxy stereo API), test\convtest.cpp (OpenXR -> LithTech pose conversion).

Loading the VR cshell
  The engine only loads cshell.dll/object.lto from its -rez list (avp2cmds.txt, written by the launcher);
  the retail copies live in AVP2DLL.REZ, and loose DLLs in the game folder are NOT used. install.bat copies
  deploy\vrrez\cshell.dll into <game folder>\vrrez, and the VR launchers run lithtech.exe -cmdfile avp2cmds.txt
  -rez vrrez from the game folder; later -rez entries win, so that cshell.dll replaces the retail one.
  After rebuilding cshell, copy K:\Coding\Aliens-Versus-Predator-2-VR\proj\AVP2\AVP2\cshell.dll to deploy\vrrez\ and run install.bat.

HUD (API version 2)
  In stereo, cshell renders the eyes, calls SubmitStereo(eyes, hudFollows=1) and flips; then it clears to black,
  draws only the HUD, and the engine flips again. The proxy captures the first image as the projection layer and
  runs the second through a small shader (alpha = brightest channel, so black is see-through) into a HUD
  swapchain, shown head-locked in VIEW space (HudDistance/HudWidth/HudHeight in avp2xr.ini). The HUD image is
  not presented to the monitor.
  The proxy and cshell must agree on AVP2XR_API_VERSION.

Resolution (API version 3)
  dgVoodoo's forced resolution sets the D3D11 back buffer size exactly (measured with test\ddtest.cpp), while
  the game keeps its own mode (e.g. 1280x960). The forced size is (2 x per-eye width) x per-eye height, using
  the headset's recommended size, which the proxy logs at startup ("ideal dgVoodoo Resolution = ..."). If
  you change SteamVR's render resolution, update [DirectX] Resolution to match. cshell reports the game's own
  resolution via SetGameResolution so the HUD panel and flat screen keep the game's aspect ratio.

Performance
  cshell.log: "Frame timing" every 2 s (wait / eyes / eye flip / HUD / HUD flip+game). avp2xr.log: "Proxy timing"
  (xrWaitFrame, eye copy, monitor present, HUD capture, xrEndFrame). Under SteamVR the frame pacing wait is in
  xrEndFrame. avp2xr.ini DesktopMirror (default 1, ~0.15 ms per frame; 0 = don't show stereo frames on the monitor); MirrorEye
  (left/right/both, default left) shows one eye cropped to fill the monitor, for onlookers, with the HUD
  over it in the game's aspect ratio (presented at the HUD flip, so it matches the headset). Flat frames (menus) are
  redrawn in the game's aspect ratio with black bars (MirrorFlat) instead of dgVoodoo's stretch; test\flattest.cpp; the HUD swapchain
  is made at the game's resolution (the game only draws the HUD at that size).
  The big one: d3d.ren's LockOnFlip (default 1) locks the back buffer at every flip, which under dgVoodoo is a
  full GPU sync (~5 ms per flip, two flips per stereo frame). cshell sets LockOnFlip 0 when stereo starts:
  gameplay went from 22.3 ms (45 fps) to 11.1 ms (90 fps) on a Quest 2 at 90 Hz.
