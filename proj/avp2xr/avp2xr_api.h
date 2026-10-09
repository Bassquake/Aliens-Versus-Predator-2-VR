// avp2xr_api.h: interface between the game (cshell.dll) and the avp2xr d3d11.dll proxy.
//
// The proxy exports avp2xr_GetApi(). Per rendered frame the game calls BeginStereoFrame()
// before drawing, renders one view per eye into its own rectangle of the back buffer, then
// calls SubmitStereo() before flipping. The proxy's Present hook then submits those
// rectangles as an OpenXR projection layer, optionally with a HUD image from a second flip.
// Frames without a SubmitStereo() (menus, loading screens) are shown on the flat virtual
// screen instead.
//
// All values use OpenXR conventions: right-handed, +Y up, -Z forward, metres, relative to
// the LOCAL reference space.

#ifndef AVP2XR_API_H
#define AVP2XR_API_H

#define AVP2XR_API_VERSION 11

// Version 11: SetMarkerImage / SubmitMarkers
#define AVP2XR_MAX_MARKER_IMAGES	8
#define AVP2XR_MAX_MARKERS			16

// Avp2XrInput::buttons
#define AVP2XR_BTN_FIRE			0x01	// right trigger
#define AVP2XR_BTN_ALTFIRE		0x02	// right grip
#define AVP2XR_BTN_JUMP			0x04	// right A
#define AVP2XR_BTN_CROUCH		0x08	// right B
#define AVP2XR_BTN_USE			0x10	// left X (left A on Index)
#define AVP2XR_BTN_VISION		0x20	// left Y (left B on Index)
#define AVP2XR_BTN_RELOAD		0x40	// left grip
#define AVP2XR_BTN_TAUNT		0x80	// unbound unless avp2xr.ini binds it (multiplayer only)
#define AVP2XR_BTN_MENUSELECT	0x100	// right A: Enter in menus
#define AVP2XR_BTN_MENUBACK		0x200	// right B: back (Escape) in menus
#define AVP2XR_BTN_PAUSE		0x400	// left menu: Escape (opens the menu while playing)
								// 0x800 unused (was the crosshair toggle; avp2xr.ini ShowCrosshair instead)
#define AVP2XR_BTN_CLOAK		0x1000	// Predator cloak: the C key (unbound unless avp2xr.ini binds it)
#define AVP2XR_BTN_FLARE		0x2000	// Marine flare (unbound unless avp2xr.ini binds it)
#define AVP2XR_BTN_DISCRETRIEVE	0x4000	// Predator disc retrieve: the F key (unbound unless avp2xr.ini binds it)
#define AVP2XR_BTN_WALLWALK		0x8000	// Alien wall-walk while held (unbound unless avp2xr.ini binds it)
#define AVP2XR_BTN_HACK			0x10000	// the H key: Marine hacking device, Predator's H item (unbound unless bound)
#define AVP2XR_BTN_POUNCE		0x20000	// Alien pounce (unbound unless avp2xr.ini binds it)
#define AVP2XR_BTN_OBJECTIVES	0x40000	// mission objectives while held, like Tab (unbound unless bound)
#define AVP2XR_BTN_ZOOM			0x80000	// Predator zoom: next level, then back to none (unbound unless bound)
#define AVP2XR_BTN_SHOULDERLAMP	0x100000	// Marine shoulder lamp on/off (unbound unless avp2xr.ini binds it)
#define AVP2XR_BTN_TORCHSIFT	0x200000	// the T key: Marine welding torch, Predator energy sift (unbound unless bound)
#define AVP2XR_BTN_MEDICOMP		0x400000	// Predator medicomp: the G key (unbound unless avp2xr.ini binds it)
#define AVP2XR_BTN_NEXTWEAPON	0x800000	// next weapon, like right stick up (unbound unless avp2xr.ini binds it)
#define AVP2XR_BTN_PREVWEAPON	0x1000000	// previous weapon, like right stick down (unbound unless bound)
#define AVP2XR_BTN_ADJUST		0x2000000	// hand adjust mode on/off: fit the weapon in each hand with the sticks
										// (unbound unless avp2xr.ini binds it)

// SubmitStereo hudFlags
#define AVP2XR_HUD_FOLLOWS		1	// a HUD-only image follows with the next flip; it is shown
									// head-locked, in front of the viewer

struct Avp2XrView
{
	float orientation[4];  // x, y, z, w
	float position[3];
	float fov[4];          // angleLeft, angleRight, angleUp, angleDown in radians (left/down negative)
};

struct Avp2XrInput
{
	int aimValid;             // the aiming (right) controller is tracked
	float aimOrientation[4];  // its aim pose: x, y, z, w; points down its -Z like any OpenXR pose
	float aimPosition[3];
	int moveValid;            // the movement stick is bound
	float move[2];            // left thumbstick: x = right, y = forward, each -1 to 1
	int recenterCount;        // goes up each time the runtime recenters (holding the Meta/system
	                          // button); the game re-anchors its view when it changes
	int turnValid;            // the turn stick is bound
	float turn;               // right thumbstick x: -1 (left) to 1 (right), for snap turning
	float turnY;              // right thumbstick y: -1 (down) to 1 (up), for weapon cycling
	unsigned int buttons;     // AVP2XR_BTN_* held this frame
	int offValid;             // the off-hand (left) controller is tracked
	float offOrientation[4];  // its grip pose: x, y, z, w
	float offPosition[3];
	int gripValid;             // the right controller's grip pose is tracked
	float gripOrientation[4];  // its grip pose (at the palm): x, y, z, w
	float gripPosition[3];
};

struct Avp2XrEyeSubmit
{
	float fov[4];   // the FOV the eye was actually rendered with, same layout as Avp2XrView::fov
	float rect[4];  // x, y, width, height of the eye's image as fractions (0-1) of the back buffer
};

// SubmitCrosshairs, per eye
struct Avp2XrCrosshair
{
	int visible;        // drawn over this eye's image
	float center[2];    // x, y as fractions (0-1) of the back buffer
	float halfSize[2];  // half its width and height, same units
	float alpha;        // 0-1
};

// SubmitMarkers: an image on something in the world (the Predator's shoulder cannon target), in one eye
struct Avp2XrMarker
{
	int image;          // 0 to AVP2XR_MAX_MARKER_IMAGES - 1, as given to SetMarkerImage
	int eye;            // 0 = left, 1 = right: drawn inside that eye's image only
	float center[2];    // x, y as fractions (0-1) of the back buffer
	float halfSize[2];  // half its width and height before it's turned, same units
	float angle;        // radians, clockwise on the screen, about its centre
	float alpha;        // 0-1
};

struct Avp2XrApi
{
	int version;  // AVP2XR_API_VERSION

	// Waits for the next XR frame and fills views[0] (left) and views[1] (right) with the
	// predicted eye poses. Returns 0 if stereo can't be rendered this frame (no session,
	// headset not tracking, runtime not ready); render normally in that case.
	int (__cdecl* BeginStereoFrame)(Avp2XrView views[2]);

	// Call after rendering both eyes of a frame that BeginStereoFrame returned 1 for, before
	// the flip that shows them. hudFlags is a combination of AVP2XR_HUD_* flags. With
	// AVP2XR_HUD_FOLLOWS the game then clears the screen, draws only the HUD and flips again;
	// that second image becomes a transparent HUD panel (black is see-through) and isn't shown
	// on the monitor.
	void (__cdecl* SubmitStereo)(const Avp2XrEyeSubmit eyes[2], int hudFlags);

	// Call every frame (menus too) with the game's screen size. dgVoodoo may render at a forced
	// resolution with a different aspect ratio; the flat screen and HUD panel use this one so
	// 2D graphics aren't stretched.
	void (__cdecl* SetGameResolution)(int width, int height);

	// Controller state from the latest XR frame, stereo or not (menus too; poses are predicted
	// for that frame). Returns 0 if there's no controller input at all; recenterCount is
	// filled in either way.
	int (__cdecl* GetInput)(Avp2XrInput* input);

	// Version 10 on (a game built for version 9 doesn't use these):

	// The crosshair image: width x height pixels, each 0xAARRGGBB (alpha 0 = see-through). It's
	// copied; call again when it changes.
	void (__cdecl* SetCrosshairImage)(int width, int height, const unsigned int* pixels);

	// Per stereo frame, before SubmitStereo: the latest crosshair image is drawn over each eye's
	// image at exactly these places, filtered, at the headset's resolution. (The game's own 2D
	// drawing snaps to its pixels, several headset pixels each, so a crosshair it draws moves in
	// steps.) Not called = no crosshair that frame.
	void (__cdecl* SubmitCrosshairs)(const Avp2XrCrosshair crosshairs[2]);

	// Version 11 on:

	// Marker image number image (0 to AVP2XR_MAX_MARKER_IMAGES - 1): width x height pixels, each
	// 0xAARRGGBB. It's copied; call again when it changes. pixels = null removes it.
	void (__cdecl* SetMarkerImage)(int image, int width, int height, const unsigned int* pixels);

	// Per stereo frame, before SubmitStereo: up to AVP2XR_MAX_MARKERS images drawn over the eyes
	// like the crosshair (under it), at the headset's resolution. Not called = none that frame.
	void (__cdecl* SubmitMarkers)(const Avp2XrMarker* markers, int count);
};

typedef const Avp2XrApi* (__cdecl* PFN_avp2xr_GetApi)();

#endif
