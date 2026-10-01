// ----------------------------------------------------------------------- //
//
// MODULE  : VRMgr.cpp
//
// PURPOSE : Stereo rendering for VR through the avp2xr d3d11.dll proxy
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "VRMgr.h"
#include "GameClientShell.h"
#include "CommandIds.h"
#include "MsgIds.h"
#include "vartrack.h"
#include "MuzzleFlashFX.h"
#include "ClientButeMgr.h"
#include "CrosshairMgr.h"
#include "CharacterFuncs.h"
#include <math.h>
#include <stdarg.h>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

static VarTrack		g_vtVREnable;
static VarTrack		g_vtVRWorldScale;
static VarTrack		g_vtVRSnapTurn;
static VarTrack		g_vtVRGunScale;
static VarTrack		g_vtVRGunMode;
static VarTrack		g_vtVRFlashDepth;
static VarTrack		g_vtVRFlashScale;
static VarTrack		g_vtVRTwoHanded;
static VarTrack		g_vtVRGunOffsetX;
static VarTrack		g_vtVRGunOffsetY;
static VarTrack		g_vtVRGunOffsetZ;
static VarTrack		g_vtVRGripX;
static VarTrack		g_vtVRGripY;
static VarTrack		g_vtVRGripZ;

// Human characters are about 100 units tall (DefaultDims half-height ~50-55), so a
// 1.8 m person gives roughly 55 units per metre.
#define VR_DEFAULT_WORLD_SCALE	55.0f

// How often to look for the proxy while it isn't loaded yet (dgVoodoo loads d3d11.dll late).
#define VR_LOOKUP_INTERVAL_MS	2000

// Movement stick dead zone (fraction of full deflection).
#define VR_STICK_DEADZONE		0.2f

// Snap turn: the turn stick triggers past PRESS and must come back under RELEASE before the
// next turn, so one push is one turn.
#define VR_SNAP_PRESS			0.7f
#define VR_SNAP_RELEASE			0.3f

// Smooth turn speed at full push, degrees per second (avp2xr.ini SmoothTurnSpeed)
#define VR_DEFAULT_SMOOTH_TURN	120.0f

// Weapon cycling opens the game's weapon chooser, which normally waits for the fire button.
// This long after the last push on the stick, the highlighted weapon is selected.
#define VR_CYCLE_SELECT_MS		600

// Keyboard scancode the Cloak button presses (the Predator cloak's key, C)
#define VR_CLOAK_SCANCODE		0x2E

// ...and the DiscRetrieve button (the Predator disc retrieve's key, F). F is the Marine's flare
// too, so it's only pressed for a Predator.
#define VR_DISCRETRIEVE_SCANCODE	0x21

// ...and the Hack button (H: the Marine's hacking device and the Predator's H item; the Alien has
// nothing on H)
#define VR_HACK_SCANCODE		0x23

// ...and the TorchSift button (T: the Marine's welding torch and the Predator's energy sift; the
// Alien has nothing on T)
#define VR_TORCHSIFT_SCANCODE	0x14

// ...and the Medicomp button (G, the Predator's medicomp). G is the Marine's shoulder lamp and the
// Alien's wall-walk toggle too, so it's only pressed for a Predator.
#define VR_MEDICOMP_SCANCODE	0x22

// Presses (bDown) or releases a key by its scancode, as if on the keyboard: for commands only the
// (retail) game server handles, which the engine sends it from the keys bound to them
static void SendKey(WORD nScancode, LTBOOL bDown)
{
	INPUT key;
	memset(&key, 0, sizeof(key));
	key.type = INPUT_KEYBOARD;
	key.ki.wScan = nScancode;
	key.ki.dwFlags = KEYEVENTF_SCANCODE | (bDown ? 0 : KEYEVENTF_KEYUP);
	SendInput(1, &key, sizeof(key));
}

// Menus: a stick pushed past MENU_PRESS acts as an arrow key until it's back under MENU_RELEASE,
// repeating like a held key (for sliders and long lists)
#define VR_MENU_PRESS			0.6f
#define VR_MENU_RELEASE			0.3f
#define VR_MENU_REPEAT_DELAY_MS	400
#define VR_MENU_REPEAT_MS		120

// The first-person models are built tiny, for their really-close projection: the marine hand
// is 0.31 units from wrist to middle knuckle (a real one is ~5 at 55 units/m) and the pulse
// rifle 2.76 units from wrist to muzzle (~27 real). Drawn in the world they are scaled up by this.
#define VR_DEFAULT_GUN_SCALE	12.0f

// The model's palm goes on the controller's grip pose (the palm). If only the aim pose is
// tracked, which sits at the front of the controller, the palm goes this far (metres) back
// along it (measured in the headset with Touch controllers).
#define VR_AIM_TO_PALM			0.2f

// Where the right palm is in a first-person model (model space, before scaling: right, up,
// forward) when the model has no hand nodes to find it by (shotgun, minigun, smartgun,
// exosuit...). Near the pulse rifle's palm, (0.13, -0.19, -0.25).
#define VR_DEFAULT_GRIP_X		0.1f
#define VR_DEFAULT_GRIP_Y		-0.2f
#define VR_DEFAULT_GRIP_Z		0.0f

// How far the aim is followed to find what it hits, for the crosshair (world units)
#define VR_CROSSHAIR_RANGE		10000.0f

// Weapons (Weapons.txt names) that get no crosshair in VR
static const char *s_szNoCrosshairWeapons[] = { "Knife" };

static LTBOOL WeaponHasCrosshair(const WEAPON *pWeapon)
{
	if(!pWeapon)
		return LTFALSE;
	for(int i = 0; i < sizeof(s_szNoCrosshairWeapons) / sizeof(s_szNoCrosshairWeapons[0]); i++)
		if(!stricmp(pWeapon->szName, s_szNoCrosshairWeapons[i]))
			return LTFALSE;
	return LTTRUE;
}

// The aim ray for the crosshair passes through the player and the weapon model
extern LTBOOL BodyPropListFilterFn(HOBJECT hTest, void *pUserData);
static LTBOOL CrosshairFilterFn(HOBJECT hObj, void *pUserData)
{
	if(hObj == g_pLTClient->GetClientObject() || hObj == (HOBJECT)pUserData)
		return LTFALSE;
	return BodyPropListFilterFn(hObj, pUserData);
}

// Right wrist and middle-finger base node names used by the first-person models
static const char *s_szWristNodes[] = { "Wrist_r1", "_zN_Wrist_r1", "r_wrist", "_zN_r_wrist" };
static const char *s_szKnuckleNodes[] = { "Middle_r1", "_zN_Middle_r1", "r_middle1", "_zN_r_middle1" };

// The left arm in the first-person models: the Marine ones (forearm jnt66_1, wrist jnt66_2, middle
// knuckle Middle_r5) and the Alien and Predator ones (ll_arm, l_wrist, l_middle1). The root takes
// the whole arm with it, hand and fingers (and the Predator's wrist computer) included.
static const char *s_szLeftArmNodes[] = { "jnt66_1", "_zN_jnt66_1", "ll_arm", "_zN_ll_arm" };
static const char *s_szLeftWristNodes[] = { "jnt66_2", "_zN_jnt66_2", "l_wrist", "_zN_l_wrist" };
static const char *s_szLeftKnuckleNodes[] = { "Middle_r5", "_zN_Middle_r5", "l_middle1", "_zN_l_middle1" };
static const char *s_szLeftPointerNodes[] = { "Pointer_r5", "_zN_Pointer_r5", "l_pointer1", "_zN_l_pointer1" };
static const char *s_szLeftPinkyNodes[] = { "Pinky_r5", "_zN_Pinky_r5", "l_pinky1", "_zN_l_pinky1" };
static const char **s_pszLeftHandNodes[4] = { s_szLeftWristNodes, s_szLeftKnuckleNodes, s_szLeftPointerNodes, s_szLeftPinkyNodes };

// First-person models whose left arm is in pieces of its own (and no other piece uses its nodes),
// from the model files (tools: jobs abcpieces/leftpieces): the model, its left-arm pieces and its
// other pieces. These draw the arm as a second copy of the model at the left controller.
struct ArmPieces
{
	const char	*szModel;
	const char	*szLeft;
	const char	*szOther;
};
static const ArmPieces s_ArmPieces[] =
{
	{ "aclaws_pv.abc", "l_arm l_thumb", "r_arm r_thumb" },
	{ "aeat_pv.abc", "l_arm l_thumb", "r_arm r_thumb" },
	{ "afacehug_pv.abc", "l_arm l_thumb", "r_arm r_thumb" },
	{ "apouncejump_pv.abc", "l_arm l_thumb", "r_arm r_thumb" },
	{ "apounce_pv.abc", "l_arm l_thumb", "r_arm r_thumb" },
	{ "atear_pv.abc", "l_arm l_thumb", "r_arm r_thumb" },
	{ "mrailgun_pv.abc", "thumb1_zTex1", "square1_39 scope3_zTex2" },
	{ "mshotgun_pv.abc", "mainarm1_zTex1", "Shotgun1" },
	{ "msmartgun_pv.abc", "mainarm1_zTex1", "bmerge1_2" },
	{ "phackingdevice_pv.abc", "ll_arm_zTex0 l_hand_zTex0 l_thumb_zTex0 symbols_zTex3", "door1_zTex0 door2_zTex0 r_hand_zTex1 r_thumb_zTex1 rl_arm_zTex1" },
	{ "photbomb_pv.abc", "ll_arm l_hand l_thumb", "door1 door2 r_thumb rl_arm r_hand Bomb symbols" },
	{ "pmedicomp_pv.abc", "ll_arm_zTex0 l_hand_zTex0 l_thumb_zTex0 symbols_zTex4", "door1_zTex0 door2_zTex0 r_hand_zTex1 r_thumb_zTex1 rl_arm_zTex1 display_zTex3 medicomp2_zTex2 medicomp1_zTex2 medicomp3_zTex2 medicomp4_zTex2 medicomp4_zTex3 medicomp3_zTex5" },
	{ "pshouldercannon_pv.abc", "ll_arm_zTex0 l_hand_zTex0 l_thumb_zTex0 symbols_zTex3", "door1_zTex0 door2_zTex0 r_hand_zTex1 r_thumb_zTex1 rl_arm_zTex1" },
	{ "pspear_pv.abc", "ll_arm_zTex0", "rl_arm_zTex1 spear2_zTex3" },
	{ "mflamer_pv.abc", "thumb1_zTex1_left", "cube4 thumb1_zTex1" },
	{ "mgrenade_pv.abc", "sillyinder_zTex2_left", "cube4_zTex0 sillyinder_zTex2 Canister_zTex2 projectile3 projectile4 projectile5 projectile0 projectile1 projectile2" },
	{ "mknife_pv.abc", "mainarm_zTex1_left", "Stitch1 mainarm_zTex1" },
	{ "mpistol_pv.abc", "thumb1_zTex1_left", "cube6 thumb1_zTex1" },
	{ "msadar_pv.abc", "thumb_zTex1_left", "cyl30_zTex0 thumb_zTex1" },
	{ "mpulserifle_pv.abc", "thumb_left", "Pulse_Rifle_3d thumb" },
	{ "pspeargun_pv.abc", "ll_arm_zTex0_left", "ll_arm_zTex0 rl_arm_zTex1 Stitch4_zTex3" },
	{ "pdisc_pv.abc", "ll_arm_zTex0_left", "ll_arm_zTex0 rl_arm_zTex1 cyl2_zTex3" },
	{ "pnetgun_pv.abc", "ll_arm_zTex0_left", "ll_arm_zTex0 r_thumb_zTex1 Stitch3_zTex3" },
	{ "penergysift_pv.abc", "ll_arm_zTex0_left", "ll_arm_zTex0 r_thumb_zTex1 sphere4_zTex3" },
	// the Marine and Predator ones above with "_left" pieces are split by tools/splitarms.py (the copies in vrrez)
	{ "pwristblades_pv.abc", "ll_arm_zTex0 l_hand_zTex0 l_thumb_zTex0", "door1_zTex0 door2_zTex0 rl_arm_zTex1 r_hand_zTex1 r_thumb_zTex1 blade2_zTex1 blade1_zTex1" },
};

// The space-separated names in szList, into aNames; returns how many
static int SplitNames(const char *szList, char aNames[][48], int nMax)
{
	int n = 0;
	while(*szList && n < nMax)
	{
		while(*szList == ' ')
			szList++;
		int i = 0;
		while(*szList && *szList != ' ' && i < 47)
			aNames[n][i++] = *szList++;
		aNames[n][i] = 0;
		if(i)
			n++;
		while(*szList && *szList != ' ')
			szList++;
	}
	return n;
}

// The right hand's pointer and pinky knuckles, for the way the hand is turned
static const char *s_szPointerNodes[] = { "Pointer_r1", "_zN_Pointer_r1", "r_pointer1", "_zN_r_pointer1" };
static const char *s_szPinkyNodes[] = { "Pinky_r1", "_zN_Pinky_r1", "r_pinky1", "_zN_r_pinky1" };

// Muzzle sockets of the first-person models, and for models without one, how far along the
// line from the flat-screen eye to the game's muzzle flash position the muzzle really is. The
// flash positions (MuzzlePos in Weapons.txt) only look right from that eye: they're at z = 10,
// while the pulse rifle's Flash socket is 0.27 of the way there and the pistol's barrel end
// 0.24 (from their models and Pos offsets).
static const char *s_szMuzzleSockets[] = { "Flash", "Muzzle", "Socket0" };
#define VR_DEFAULT_FLASH_DEPTH	0.24f

// The flash is sized for the game's flash position, so moved in to the muzzle it's shrunk by
// how much nearer that is, which keeps its flat-screen size next to the gun. In VR you sight
// down the barrel, so it's shrunk by this as well to keep it out of the way.
#define VR_DEFAULT_FLASH_SCALE	0.5f

// Model-space position of the first of the named sockets the model has
static LTBOOL FindModelSocket(HOBJECT hObj, const char **szNames, int nNames, LTVector &vPos)
{
	ModelLT *pModelLT = g_pLTClient->GetModelLT();
	for(int i = 0; i < nNames; i++)
	{
		HMODELSOCKET hSocket;
		LTransform tTransform;
		if(pModelLT->GetSocket(hObj, (char*)szNames[i], hSocket) == LT_OK &&
		   pModelLT->GetSocketTransform(hObj, hSocket, tTransform, LTFALSE) == LT_OK)
		{
			g_pLTClient->GetTransformLT()->GetPos(tTransform, vPos);
			return LTTRUE;
		}
	}
	return LTFALSE;
}

// Model-space position of the first of the named nodes the model has
static LTBOOL FindModelNode(HOBJECT hObj, const char **szNames, int nNames, LTVector &vPos)
{
	ModelLT *pModelLT = g_pLTClient->GetModelLT();
	for(int i = 0; i < nNames; i++)
	{
		HMODELNODE hNode;
		LTransform tTransform;
		if(pModelLT->GetNode(hObj, (char*)szNames[i], hNode) == LT_OK &&
		   pModelLT->GetNodeTransform(hObj, hNode, tTransform, LTFALSE) == LT_OK)
		{
			g_pLTClient->GetTransformLT()->GetPos(tTransform, vPos);
			return LTTRUE;
		}
	}
	return LTFALSE;
}

// The first of the named nodes the model has, or INVALID_MODEL_NODE
static HMODELNODE FindNodeHandle(HOBJECT hObj, const char **szNames, int nNames, const char **pszFound)
{
	ModelLT *pModelLT = g_pLTClient->GetModelLT();
	for(int i = 0; i < nNames; i++)
	{
		HMODELNODE hNode;
		if(pModelLT->GetNode(hObj, (char*)szNames[i], hNode) == LT_OK)
		{
			if(pszFound)
				*pszFound = szNames[i];
			return hNode;
		}
	}
	return INVALID_MODEL_NODE;
}

// ----------------------------------------------------------------------- //
// OpenXR is right-handed with -Z forward; LithTech is left-handed with +Z forward. Mirroring
// Z maps a position (x, y, z) to (x, y, -z) and a rotation quaternion (x, y, z, w) to
// (-x, -y, z, w).

static LTRotation FromXr(const float q[4])
{
	return LTRotation(-q[0], -q[1], q[2], q[3]);
}

// An OpenXR position relative to a reference point, as a LithTech vector (still in metres)
static LTVector FromXrPos(const float p[3], const float ref[3])
{
	return LTVector(p[0] - ref[0], p[1] - ref[1], -(p[2] - ref[2]));
}

static const float s_fNoRef[3] = { 0.0f, 0.0f, 0.0f };

static LTRotation YawRotation(LTFLOAT fYaw)
{
	return LTRotation(0.0f, (LTFLOAT)sin(fYaw * 0.5f), 0.0f, (LTFLOAT)cos(fYaw * 0.5f));
}

// Yaw of a rotation, taken from its right vector, which stays level while it pitches.
static LTFLOAT YawOf(const LTRotation &rRot)
{
	LTVector vR, vU, vF;
	LTRotation r = rRot;
	g_pLTClient->GetMathLT()->GetRotationVectors(r, vR, vU, vF);
	return (LTFLOAT)atan2(-vR.z, vR.x);
}

// Two-handed aiming engages when the off hand is in front of the gun hand: between these
// distances (metres) and within this angle of where the gun hand points. It lets go outside
// the wider limits, so it doesn't flicker at the edges.
#define VR_TWOHAND_MIN			0.15f
#define VR_TWOHAND_MAX			0.75f
#define VR_TWOHAND_ANGLE		40.0f
#define VR_TWOHAND_RELEASE_MIN	0.10f
#define VR_TWOHAND_RELEASE_MAX	0.90f
#define VR_TWOHAND_RELEASE_ANGLE 60.0f

// Standard (right-hand rule) cross product. LTVector::Cross returns the reverse.
static LTVector CrossStd(const LTVector &a, const LTVector &b)
{
	return LTVector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

// Rotation whose right/up/forward vectors are vR/vU/vF (orthonormal): the rotation matrix has
// them as columns (see quat_GetVectors). Tested against LithTech's math in avp2xr\test\convtest.
static LTRotation QuatFromBasis(const LTVector &vR, const LTVector &vU, const LTVector &vF)
{
	float m00 = vR.x, m10 = vR.y, m20 = vR.z, m01 = vU.x, m11 = vU.y, m21 = vU.z, m02 = vF.x, m12 = vF.y, m22 = vF.z;
	float q[4], t = m00 + m11 + m22;
	if(t > 0.0f)				{ float s = (float)sqrt(t + 1.0f) * 2.0f; q[3] = 0.25f * s; q[0] = (m21 - m12) / s; q[1] = (m02 - m20) / s; q[2] = (m10 - m01) / s; }
	else if(m00 > m11 && m00 > m22)	{ float s = (float)sqrt(1.0f + m00 - m11 - m22) * 2.0f; q[3] = (m21 - m12) / s; q[0] = 0.25f * s; q[1] = (m01 + m10) / s; q[2] = (m02 + m20) / s; }
	else if(m11 > m22)			{ float s = (float)sqrt(1.0f + m11 - m00 - m22) * 2.0f; q[3] = (m02 - m20) / s; q[0] = (m01 + m10) / s; q[1] = 0.25f * s; q[2] = (m12 + m21) / s; }
	else						{ float s = (float)sqrt(1.0f + m22 - m00 - m11) * 2.0f; q[3] = (m10 - m01) / s; q[0] = (m02 + m20) / s; q[1] = (m12 + m21) / s; q[2] = 0.25f * s; }
	return LTRotation(q[0], q[1], q[2], q[3]);
}

// The gun's angle in the hand, in the controller's own frame (degrees): pitch + tilts it up, yaw
// + turns it right, roll + rolls it clockwise (right side down). Roll first, then pitch, then yaw.
static LTRotation GunAngleRotation(LTFLOAT fPitch, LTFLOAT fYaw, LTFLOAT fRoll)
{
	LTFLOAT p = MATH_DEGREES_TO_RADIANS(fPitch), y = MATH_DEGREES_TO_RADIANS(fYaw), r = MATH_DEGREES_TO_RADIANS(fRoll);
	LTFLOAT sp = (LTFLOAT)sin(p), cp = (LTFLOAT)cos(p), sy = (LTFLOAT)sin(y), cy = (LTFLOAT)cos(y), sr = (LTFLOAT)sin(r), cr = (LTFLOAT)cos(r);
	LTRotation rPitch = QuatFromBasis(LTVector(1.0f, 0.0f, 0.0f), LTVector(0.0f, cp, -sp), LTVector(0.0f, sp, cp));
	LTRotation rYaw = QuatFromBasis(LTVector(cy, 0.0f, -sy), LTVector(0.0f, 1.0f, 0.0f), LTVector(sy, 0.0f, cy));
	LTRotation rRoll = QuatFromBasis(LTVector(cr, -sr, 0.0f), LTVector(sr, cr, 0.0f), LTVector(0.0f, 0.0f, 1.0f));
	return rYaw * rPitch * rRoll;
}

static LTFLOAT GetConsoleFloat(const char *szName)
{
	HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar((char*)szName);
	return hVar ? g_pLTClient->GetVarValueFloat(hVar) : 0.0f;
}

static void SetConsoleFloat(const char *szName, LTFLOAT fValue)
{
	char szBuffer[64];
	sprintf(szBuffer, "%s %f", szName, fValue);
	g_pLTClient->RunConsoleString(szBuffer);
}

static LTVector Rotate(const LTRotation &rRot, const LTVector &v)
{
	LTVector vR, vU, vF;
	LTRotation r = rRot;
	g_pLTClient->GetMathLT()->GetRotationVectors(r, vR, vU, vF);
	return vR * v.x + vU * v.y + vF * v.z;
}

// A rotation seen in a mirror across its frame's X (left-right swapped): what a rotation of the
// right hand is for the left
static LTRotation MirrorX(const LTRotation &r)
{
	return LTRotation(r.m_Quat[0], -r.m_Quat[1], -r.m_Quat[2], r.m_Quat[3]);
}

// A hand's frame from its knuckles: forward from the wrist to the middle knuckle, right from the
// pinky knuckle to the pointer one (square to forward), up from those. A left hand, being a
// mirror image, comes out as the mirror of a right hand held the same way.
struct HandVectors { LTVector vR, vU, vF; };

static LTBOOL GetHandVectors(const LTVector &vWrist, const LTVector &vMiddle, const LTVector &vPointer, const LTVector &vPinky,
							 HandVectors &hand)
{
	hand.vF = vMiddle - vWrist;
	if(hand.vF.MagSqr() < 0.000001f)
		return LTFALSE;
	hand.vF.Norm();
	hand.vR = vPointer - vPinky;
	hand.vR -= hand.vF * hand.vR.Dot(hand.vF);
	if(hand.vR.MagSqr() < 0.000001f)
		return LTFALSE;
	hand.vR.Norm();
	hand.vU = CrossStd(hand.vF, hand.vR);
	return LTTRUE;
}

// ----------------------------------------------------------------------- //

VRMgr::VRMgr()
{
	m_pApi = LTNULL;
	m_nNextLookupTime = 0;
	m_bStereo = LTFALSE;
	m_fEyeInsetX = m_fEyeInsetY = 0.0f;
	m_fEyeZoom = 1.0f;
	m_fZoomStrength = 1.0f;
	m_nZoomSteps = 3;
	m_bAlwaysRun = LTFALSE;
	m_bSkipCutscenes = LTFALSE;
	m_bInCutscene = LTFALSE;
	m_fNextSkipTime = 0.0f;
	m_fShownFOVX = m_fShownFOVY = 0.0f;
	m_bRendered = LTFALSE;
	m_bSubmitted = LTFALSE;
	m_bLastFrameStereo = LTFALSE;
	m_hPlayerCamera = CAMERAMGR_INVALID;
	m_bAnchorValid = LTFALSE;
	m_fAnchorYaw = 0.0f;
	memset(m_fHeadRef, 0, sizeof(m_fHeadRef));
	m_bInput = LTFALSE;
	m_nRecenterCount = 0;
	m_bSnapArmed = LTTRUE;
	m_bCycleArmed = LTTRUE;
	m_nCycleSelectTime = 0;
	m_bMenuInput = LTFALSE;
	m_bCrosshairHit = LTFALSE;
	m_bCrosshairEye[0] = m_bCrosshairEye[1] = LTFALSE;
	m_nCrosshairImageId = 0;
	m_bShowCrosshair = LTTRUE;
	m_bSmoothCrosshair = LTTRUE;
	m_nRailScope = 1;
	m_fRailScale = 1.5f;
	m_fRailDistance = 0.0f;
	m_fRailSize = 0.0f;
	m_hRailOverlay = LTNULL;
	m_vRailPos.Init();
	m_vRailScale.Init(1.0f, 1.0f, 1.0f);
	m_bWeaponIdleAnims = LTFALSE;
	m_bSmoothTurn = LTTRUE;
	m_fSmoothTurnSpeed = VR_DEFAULT_SMOOTH_TURN;
	m_rGunAngle.Init();
	m_rGunAngleCur.Init();
	m_vGunOffsetCur.Init();
	m_nMenuButtons = 0;
	m_nMenuKey = 0;
	m_nMenuRepeatTime = 0;
	m_nIgnoreButtons = 0;
	m_bTwoHanded = LTFALSE;
	m_nButtons = 0;
	m_bObjectivesOn = LTFALSE;
	m_bLastFrameInput = LTFALSE;
	m_fHeadYaw = 0.0f;
	m_bAimWorld = LTFALSE;
	m_bLastFrameAimWorld = LTFALSE;
	m_bMuzzleWorld = LTFALSE;
	m_bLastFrameMuzzle = LTFALSE;
	m_bPlayerView = LTFALSE;
	m_bLastFramePlayerView = LTFALSE;
	m_rHeadWorld.Init();
	m_bWeaponMoved = LTFALSE;
	m_bWeaponView = LTFALSE;
	memset(m_Prof, 0, sizeof(m_Prof));
	m_ProfLastPre.QuadPart = m_ProfLastPost.QuadPart = 0;
	memset(m_fProfSum, 0, sizeof(m_fProfSum));
	m_fProfMaxFrame = 0.0;
	m_nProfFrames = 0;
	m_nProfStart = 0;
	m_bGripWorld = LTFALSE;
	m_pGripWeapon = LTNULL;
	m_bGripHeld = LTFALSE;
	m_bLeftArm = LTTRUE;
	m_vLeftOffset.Init();
	m_rLeftAngle.Init();
	m_bOffWorld = LTFALSE;
	m_vOffPos.Init();
	m_rOffRot.Init();
	m_bLeftMatch = LTTRUE;
	m_bArmForcePose = LTTRUE;
	m_szIni[0] = 0;
	m_vArmOffset.Init();
	m_rArmAngle.Init();
	m_fTwoHandDown = 0.0f;
	m_fTwoHandBlend = 0.0f;
	m_rGripToAim.Init();
	m_hArmModel = LTNULL;
	m_pArmWeapon = LTNULL;
	m_hArmRoot = m_hArmRef = INVALID_MODEL_NODE;
	for(int h = 0; h < 4; h++)
	{
		m_hArmHand[h] = INVALID_MODEL_NODE;
		m_vArmSeen[h].Init();
	}
	m_bArmMatch = LTFALSE;
	m_hArmCopy = LTNULL;
	m_nArmSplit = -1;
	m_bArmCopyPieces = LTFALSE;
	m_nArmMainPieces = 0;
	m_bArmCopyActive = LTFALSE;
	m_vArmCopyOrigin.Init();
	m_rArmCopyRot.Init();
	m_rArmHand.Init();
	m_rArmMirror.Init();
	m_bArmActive = m_bArmApplied = m_bArmPass = LTFALSE;
	m_nArmTime = 0;
	m_bArmDrawing = LTFALSE;
	m_nArmPasses[0] = m_nArmPasses[1] = 0;
	m_nArmNudge = 1;
	m_nArmForced = -1;
	m_mArm.Identity();
	m_mArmInv.Identity();
	m_mArmRefInv.Identity();
	m_mArmPass.Identity();
	m_mArmPassInv.Identity();
	m_mArmPassMove.Identity();
	m_bArmSeen = LTFALSE;
	m_bArmHeld = LTFALSE;
	m_vArmPalm.Init();
	m_fExtraFOVX = m_fExtraFOVY = 0.0f;
	m_fWeaponScale = 1.0f;
	memset(&m_Input, 0, sizeof(m_Input));
	m_fTanHalfX = 0.0f;
	m_fTanHalfY = 0.0f;
	m_nScreenWidth = 0;
	m_nScreenHeight = 0;
	memset(m_Views, 0, sizeof(m_Views));
	memset(m_Submit, 0, sizeof(m_Submit));
}

// ----------------------------------------------------------------------- //

void VRMgr::Init()
{
	g_vtVREnable.Init(g_pLTClient, "VREnable", LTNULL, 1.0f);
	g_vtVRWorldScale.Init(g_pLTClient, "VRWorldScale", LTNULL, VR_DEFAULT_WORLD_SCALE);
	g_vtVRSnapTurn.Init(g_pLTClient, "VRSnapTurn", LTNULL, 45.0f);
	g_vtVRGunScale.Init(g_pLTClient, "VRGunScale", LTNULL, VR_DEFAULT_GUN_SCALE);
	g_vtVRGunMode.Init(g_pLTClient, "VRGunMode", LTNULL, 1.0f);
	g_vtVRFlashDepth.Init(g_pLTClient, "VRFlashDepth", LTNULL, VR_DEFAULT_FLASH_DEPTH);
	g_vtVRFlashScale.Init(g_pLTClient, "VRFlashScale", LTNULL, VR_DEFAULT_FLASH_SCALE);
	g_vtVRTwoHanded.Init(g_pLTClient, "VRTwoHanded", LTNULL, 1.0f);
	g_vtVRGunOffsetX.Init(g_pLTClient, "VRGunOffsetX", LTNULL, 0.0f);
	g_vtVRGunOffsetY.Init(g_pLTClient, "VRGunOffsetY", LTNULL, 0.0f);
	g_vtVRGunOffsetZ.Init(g_pLTClient, "VRGunOffsetZ", LTNULL, 0.0f);
	g_vtVRGripX.Init(g_pLTClient, "VRGripX", LTNULL, VR_DEFAULT_GRIP_X);
	g_vtVRGripY.Init(g_pLTClient, "VRGripY", LTNULL, VR_DEFAULT_GRIP_Y);
	g_vtVRGripZ.Init(g_pLTClient, "VRGripZ", LTNULL, VR_DEFAULT_GRIP_Z);
}

// ----------------------------------------------------------------------- //
// The proxy is loaded under the name d3d11.dll next to the real system d3d11.dll, so look
// for the module that exports avp2xr_GetApi rather than trusting GetModuleHandle.

LTBOOL VRMgr::FindApi()
{
	if(m_pApi)
		return LTTRUE;

	uint32 nNow = GetTickCount();
	if((int32)(nNow - m_nNextLookupTime) < 0)
		return LTFALSE;
	m_nNextLookupTime = nNow + VR_LOOKUP_INTERVAL_MS;

	HMODULE hModules[512];
	DWORD nBytes = 0;
	if(!EnumProcessModules(GetCurrentProcess(), hModules, sizeof(hModules), &nBytes))
		return LTFALSE;

	uint32 nCount = nBytes / sizeof(HMODULE);
	if(nCount > 512) nCount = 512;
	for(uint32 i = 0; i < nCount; i++)
	{
		PFN_avp2xr_GetApi pGetApi = (PFN_avp2xr_GetApi)GetProcAddress(hModules[i], "avp2xr_GetApi");
		if(!pGetApi)
			continue;

		const Avp2XrApi *pApi = pGetApi();
		// Version 9 (without the crosshair calls) works too; the crosshair is then drawn here
		if(pApi && pApi->version >= 9 && pApi->version <= AVP2XR_API_VERSION)
		{
			m_pApi = pApi;
			m_nCrosshairImageId = 0;
			g_pLTClient->CPrint("VR: found avp2xr (API version %d)", pApi->version);

			// Settings the game side uses, from avp2xr.ini next to the proxy
			char szIni[MAX_PATH];
			if(GetModuleFileNameA(hModules[i], szIni, MAX_PATH))
			{
				char *pSlash = strrchr(szIni, '\\');
				strcpy(pSlash ? pSlash + 1 : szIni, "avp2xr.ini");
				char szValue[16];
				GetPrivateProfileStringA("VR", "ShowCrosshair", "yes", szValue, sizeof(szValue), szIni);
				m_bShowCrosshair = !strchr("nN0fF", szValue[0]) || !szValue[0];
				GetPrivateProfileStringA("VR", "CrosshairSmoothing", "yes", szValue, sizeof(szValue), szIni);
				m_bSmoothCrosshair = !szValue[0] || !strchr("nN0fF", szValue[0]);

				// The message text (pickups, mission messages) bigger: it's a small system font, a few
				// pixels high at the game's resolution
				GetPrivateProfileStringA("VR", "MessageTextSize", "1.5", szValue, sizeof(szValue), szIni);
				LTFLOAT fMessageSize = (LTFLOAT)atof(szValue);
				if(g_pMessageMgr && fMessageSize > 0.0f)
					g_pMessageMgr->SetFontScale(fMessageSize);
				VRLog("Settings: CrosshairSmoothing=%s MessageTextSize=%.2f", m_bSmoothCrosshair ? "yes" : "no", fMessageSize);

				// The railgun zoom's green overlay on the gun instead of the view
				GetPrivateProfileStringA("VR", "ZoomStrength", "1", szValue, sizeof(szValue), szIni);
				m_fZoomStrength = (LTFLOAT)atof(szValue);
				if(m_fZoomStrength < 0.0f) m_fZoomStrength = 0.0f;
				if(m_fZoomStrength > 1.0f) m_fZoomStrength = 1.0f;
				GetPrivateProfileStringA("VR", "ZoomSteps", "3", szValue, sizeof(szValue), szIni);
				m_nZoomSteps = atoi(szValue);
				VRLog("Settings: ZoomStrength=%.2f ZoomSteps=%d", m_fZoomStrength, m_nZoomSteps);
				GetPrivateProfileStringA("VR", "SpeedThroughCutscenes", "no", szValue, sizeof(szValue), szIni);
				m_bSkipCutscenes = szValue[0] && !strchr("nN0fF", szValue[0]);
				VRLog("Settings: SpeedThroughCutscenes=%s", m_bSkipCutscenes ? "yes" : "no");
				GetPrivateProfileStringA("VR", "AlwaysRun", "no", szValue, sizeof(szValue), szIni);
				m_bAlwaysRun = szValue[0] && !strchr("nN0fF", szValue[0]);
				VRLog("Settings: AlwaysRun=%s", m_bAlwaysRun ? "yes" : "no");

				// The logo videos at startup: the game's own NoMovies switch (checked before each video,
				// which starts after the splash screen, so this is set in time)
				GetPrivateProfileStringA("VR", "SkipIntroVideos", "no", szValue, sizeof(szValue), szIni);
				LTBOOL bSkipIntro = szValue[0] && !strchr("nN0fF", szValue[0]);
				SetConsoleFloat("NoMovies", bSkipIntro ? 1.0f : 0.0f);
				VRLog("Settings: SkipIntroVideos=%s", bSkipIntro ? "yes" : "no");
				GetPrivateProfileStringA("VR", "RailScope", "head", szValue, sizeof(szValue), szIni);
				m_nRailScope = !_stricmp(szValue, "gun") ? 2 : (!szValue[0] || !strchr("nN0fF", szValue[0])) ? 1 : 0;
				GetPrivateProfileStringA("VR", "RailScopeScale", "1.5", szValue, sizeof(szValue), szIni);
				m_fRailScale = (LTFLOAT)atof(szValue);
				if(m_fRailScale < 0.1f)
					m_fRailScale = 1.0f;
				GetPrivateProfileStringA("VR", "RailScopeDistance", "40", szValue, sizeof(szValue), szIni);
				LTFLOAT fRailDistance = (LTFLOAT)atof(szValue);
				GetPrivateProfileStringA("VR", "RailScopeSize", "40", szValue, sizeof(szValue), szIni);
				LTFLOAT fRailSize = (LTFLOAT)atof(szValue);
				LTFLOAT fUnitsPerCmRail = g_vtVRWorldScale.GetFloat() / 100.0f;
				m_fRailDistance = fRailDistance * fUnitsPerCmRail;
				m_fRailSize = fRailSize * fUnitsPerCmRail;
				VRLog("Settings: RailScope=%s RailScopeScale=%.2f RailScopeDistance=%.0f RailScopeSize=%.0f (cm)",
					m_nRailScope == 2 ? "gun" : m_nRailScope ? "head" : "no", m_fRailScale, fRailDistance, fRailSize);

				GetPrivateProfileStringA("VR", "WeaponIdleAnimations", "no", szValue, sizeof(szValue), szIni);
				m_bWeaponIdleAnims = szValue[0] && !strchr("nN0fF", szValue[0]);

				// Turning with the right stick
				GetPrivateProfileStringA("VR", "TurnMode", "smooth", szValue, sizeof(szValue), szIni);
				m_bSmoothTurn = _stricmp(szValue, "snap") != 0;
				GetPrivateProfileStringA("VR", "SmoothTurnSpeed", "120", szValue, sizeof(szValue), szIni);
				m_fSmoothTurnSpeed = (LTFLOAT)atof(szValue);
				GetPrivateProfileStringA("VR", "SnapTurnAngle", "45", szValue, sizeof(szValue), szIni);
				g_vtVRSnapTurn.SetFloat((LTFLOAT)atof(szValue));
				VRLog("Settings: TurnMode=%s SmoothTurnSpeed=%.0f deg/s SnapTurnAngle=%.0f deg", m_bSmoothTurn ? "smooth" : "snap",
					m_fSmoothTurnSpeed, g_vtVRSnapTurn.GetFloat());

				// Where the gun sits in the hand: centimetres along the controller (forward, up,
				// right), turned into the VRGunOffset console variables (world units)
				LTFLOAT fUnitsPerCm = g_vtVRWorldScale.GetFloat() / 100.0f;
				LTFLOAT fOffset[3];
				const char *szKeys[3] = { "GunRight", "GunUp", "GunForward" };
				VarTrack *pVars[3] = { &g_vtVRGunOffsetX, &g_vtVRGunOffsetY, &g_vtVRGunOffsetZ };
				for(int k = 0; k < 3; k++)
				{
					GetPrivateProfileStringA("VR", szKeys[k], "0", szValue, sizeof(szValue), szIni);
					fOffset[k] = (LTFLOAT)atof(szValue);
					pVars[k]->SetFloat(fOffset[k] * fUnitsPerCm);
				}
				// ...and its angle (degrees): the aim itself turns, so the gun, the shots and the
				// crosshair stay together
				LTFLOAT fAngle[3];
				const char *szAngleKeys[3] = { "GunPitch", "GunYaw", "GunRoll" };
				for(int a = 0; a < 3; a++)
				{
					GetPrivateProfileStringA("VR", szAngleKeys[a], "0", szValue, sizeof(szValue), szIni);
					fAngle[a] = (LTFLOAT)atof(szValue);
				}
				m_rGunAngle = GunAngleRotation(fAngle[0], fAngle[1], fAngle[2]);
				m_rGunAngleCur = m_rGunAngle;
				m_vGunOffsetCur = LTVector(g_vtVRGunOffsetX.GetFloat(), g_vtVRGunOffsetY.GetFloat(), g_vtVRGunOffsetZ.GetFloat());

				VRLog("Settings from %s: ShowCrosshair=%s WeaponIdleAnimations=%s GunForward=%.1f GunUp=%.1f GunRight=%.1f (cm) "
					"GunPitch=%.1f GunYaw=%.1f GunRoll=%.1f (degrees)", szIni, m_bShowCrosshair ? "yes" : "no",
					m_bWeaponIdleAnims ? "yes" : "no", fOffset[2], fOffset[1], fOffset[0], fAngle[0], fAngle[1], fAngle[2]);

				// The left arm on the left controller, fitted like the gun in the right hand: by
				// default the Gun* settings mirrored (right and yaw and roll the other way)
				GetPrivateProfileStringA("VR", "LeftArm", "yes", szValue, sizeof(szValue), szIni);
				m_bLeftArm = !szValue[0] || !strchr("nN0fF", szValue[0]);
				LTFLOAT fLeft[3] = { -fOffset[0], fOffset[1], fOffset[2] };
				const char *szLeftKeys[3] = { "LeftHandRight", "LeftHandUp", "LeftHandForward" };
				LTFLOAT fLeftAngle[3] = { fAngle[0], -fAngle[1], -fAngle[2] };
				const char *szLeftAngleKeys[3] = { "LeftHandPitch", "LeftHandYaw", "LeftHandRoll" };
				for(int l = 0; l < 3; l++)
				{
					GetPrivateProfileStringA("VR", szLeftKeys[l], "", szValue, sizeof(szValue), szIni);
					if(szValue[0])
						fLeft[l] = (LTFLOAT)atof(szValue);
					GetPrivateProfileStringA("VR", szLeftAngleKeys[l], "", szValue, sizeof(szValue), szIni);
					if(szValue[0])
						fLeftAngle[l] = (LTFLOAT)atof(szValue);
				}
				m_vLeftOffset = LTVector(fLeft[0], fLeft[1], fLeft[2]) * fUnitsPerCm;
				m_rLeftAngle = GunAngleRotation(fLeftAngle[0], fLeftAngle[1], fLeftAngle[2]);
				GetPrivateProfileStringA("VR", "TwoHandedHandDown", "6", szValue, sizeof(szValue), szIni);
				m_fTwoHandDown = (LTFLOAT)atof(szValue) * fUnitsPerCm;
				GetPrivateProfileStringA("VR", "LeftArmForcePose", "yes", szValue, sizeof(szValue), szIni);
				m_bArmForcePose = !szValue[0] || !strchr("nN0fF", szValue[0]);
				if(!m_bArmForcePose)
					VRLog("Settings: LeftArmForcePose=no (diagnostic: the idle left arm won't follow the controller)");
				GetPrivateProfileStringA("VR", "LeftHandMatch", "yes", szValue, sizeof(szValue), szIni);
				m_bLeftMatch = !szValue[0] || !strchr("nN0fF", szValue[0]);
				strncpy(m_szIni, szIni, sizeof(m_szIni) - 1);
				m_szIni[sizeof(m_szIni) - 1] = 0;
				VRLog("Settings: LeftArm=%s LeftHandMatch=%s LeftHandForward=%.1f LeftHandUp=%.1f LeftHandRight=%.1f (cm) "
					"LeftHandPitch=%.1f LeftHandYaw=%.1f LeftHandRoll=%.1f (degrees) TwoHandedHandDown=%.1f (cm)", m_bLeftArm ? "yes" : "no",
					m_bLeftMatch ? "yes" : "no", fLeft[2], fLeft[1], fLeft[0], fLeftAngle[0], fLeftAngle[1], fLeftAngle[2],
					fUnitsPerCm > 0.0f ? m_fTwoHandDown / fUnitsPerCm : 0.0f);
			}

			// The menus may already have turned the cursor on before VR was found
			CInterfaceMgr *pInterface = g_pGameClientShell->GetInterfaceMgr();
			pInterface->UseCursor(pInterface->IsCursorUsed());
		}
		else
		{
			g_pLTClient->CPrint("VR: avp2xr API version %d doesn't match the game's (%d); VR disabled",
				pApi ? pApi->version : -1, AVP2XR_API_VERSION);
			m_nNextLookupTime = nNow + 0x7fffffff;
		}
		break;
	}

	return m_pApi != LTNULL;
}

// ----------------------------------------------------------------------- //

void VRMgr::Update()
{
	if(!FindApi())
		return;

	uint32 nWidth = 0, nHeight = 0;
	g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &nWidth, &nHeight);
	m_pApi->SetGameResolution((int)nWidth, (int)nHeight);

	// Outside play (menus, pause, loading) the controllers drive the menus like the keyboard
	LTBOOL bPlaying = g_pGameClientShell->GetInterfaceMgr()->GetGameState() == GS_PLAYING;
	if(bPlaying && m_bMenuInput)
		LeaveMenus();
	UpdateButtons();
	if(!bPlaying)
		UpdateMenus();
	else
		UpdateCutsceneSkip();
}

// ----------------------------------------------------------------------- //
// avp2xr.ini SpeedThroughCutscenes: the in-engine cutscenes (the cinematic camera) are skipped as the game's
// skip key (space) does it, by sending an activate with no position. Sent a moment after the
// cutscene starts, then again each second while it's still on (a scene of several shots, or one
// that ignored it; some can't be skipped at all, as in the game).

void VRMgr::UpdateCutsceneSkip()
{
	HCAMERA hCinematic = g_pGameClientShell->GetCinematicCamera();
	LTBOOL bCutscene = m_bSkipCutscenes && hCinematic != CAMERAMGR_INVALID && g_pCameraMgr->GetCameraActive(hCinematic);
	LTFLOAT fNow = g_pLTClient->GetTime();
	if(!bCutscene)
	{
		m_bInCutscene = LTFALSE;
		return;
	}
	if(!m_bInCutscene)
	{
		m_bInCutscene = LTTRUE;
		m_fNextSkipTime = fNow + 0.3f;
	}
	if(fNow < m_fNextSkipTime)
		return;
	m_fNextSkipTime = fNow + 1.0f;

	LTVector vNull(0.0f, 0.0f, 0.0f);
	HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_ACTIVATE);
	g_pLTClient->WriteToMessageVector(hMessage, &vNull);
	g_pLTClient->WriteToMessageVector(hMessage, &vNull);
	g_pLTClient->EndMessage(hMessage);
	VRLog("Cutscene: skip sent");
}

// ----------------------------------------------------------------------- //
// The eye's view of the aim point: where on the screen, and how big the crosshair is there
// compared with the flat screen (so it covers the same angle as it does there).

void VRMgr::ProjectCrosshair(int nEye, HCAMERA hCamera, const LTVector &vEyePos, const LTRotation &rEyeRot,
							 LTFLOAT fFOVX, LTFLOAT fFOVY, const LTRect &rRect)
{
	m_bCrosshairEye[nEye] = LTFALSE;
	if(hCamera != m_hPlayerCamera || !m_bCrosshairHit || !m_nScreenWidth || !m_nScreenHeight)
		return;

	LTVector c = Rotate(rEyeRot.Conjugate(), m_vCrosshairPos - vEyePos);
	if(c.z < 1.0f)
		return;

	LTFLOAT fHalfW = (rRect.right - rRect.left) * 0.5f, fHalfH = (rRect.bottom - rRect.top) * 0.5f;
	LTFLOAT fPixX = fHalfW / (LTFLOAT)tan(fFOVX * 0.5f);		// eye pixels per unit of tangent
	LTFLOAT fPixY = fHalfH / (LTFLOAT)tan(fFOVY * 0.5f);
	LTFLOAT fFlatX = m_nScreenWidth * 0.5f / (LTFLOAT)tan(MATH_DEGREES_TO_RADIANS(FOVX_NORMAL) * 0.5f);
	LTFLOAT fFlatY = m_nScreenHeight * 0.5f / (LTFLOAT)tan(MATH_DEGREES_TO_RADIANS(FOVY_NORMAL) * 0.5f);

	m_fCrosshairX[nEye] = rRect.left + fHalfW + c.x / c.z * fPixX;
	m_fCrosshairY[nEye] = rRect.top + fHalfH - c.y / c.z * fPixY;
	m_nCrosshairX[nEye] = (int)m_fCrosshairX[nEye];
	m_nCrosshairY[nEye] = (int)m_fCrosshairY[nEye];
	m_fCrosshairScaleX[nEye] = fPixX / fFlatX;
	m_fCrosshairScaleY[nEye] = fPixY / fFlatY;
	m_bCrosshairEye[nEye] = m_nCrosshairX[nEye] >= rRect.left && m_nCrosshairX[nEye] < rRect.right &&
							m_nCrosshairY[nEye] >= rRect.top && m_nCrosshairY[nEye] < rRect.bottom;
}

// ----------------------------------------------------------------------- //

void VRMgr::DrawCrosshairs()
{
	if(!m_bCrosshairEye[0] && !m_bCrosshairEye[1])
		return;

	CCrosshairMgr *pCrosshair = g_pGameClientShell->GetInterfaceMgr()->GetCrosshairMgr();

	// avp2xr 10 on draws it, at the headset's resolution and exactly where it goes: drawn here it
	// snaps to the game's pixels (several headset pixels each) and moves in steps
	if(m_pApi->version >= 10)
	{
		Avp2XrCrosshair xh[2];
		memset(xh, 0, sizeof(xh));
		const uint32 *pPixels = LTNULL;
		uint32 nWidth = 0, nHeight = 0, nId = 0;
		LTFLOAT fHalfW = 0.0f, fHalfH = 0.0f, fAlpha = 1.0f;
		if(pCrosshair->GetVRCrosshairImage(m_bSmoothCrosshair, pPixels, nWidth, nHeight, nId, fHalfW, fHalfH, fAlpha))
		{
			if(nId != m_nCrosshairImageId)
			{
				m_pApi->SetCrosshairImage((int)nWidth, (int)nHeight, (const unsigned int *)pPixels);
				m_nCrosshairImageId = nId;
			}
			for(int nEye = 0; nEye < 2; nEye++)
			{
				if(!m_bCrosshairEye[nEye])
					continue;
				xh[nEye].visible = 1;
				xh[nEye].center[0] = m_fCrosshairX[nEye] / m_nScreenWidth;
				xh[nEye].center[1] = m_fCrosshairY[nEye] / m_nScreenHeight;
				xh[nEye].halfSize[0] = fHalfW * m_fCrosshairScaleX[nEye] / m_nScreenWidth;
				xh[nEye].halfSize[1] = fHalfH * m_fCrosshairScaleY[nEye] / m_nScreenHeight;
				xh[nEye].alpha = fAlpha;
			}
		}
		m_pApi->SubmitCrosshairs(xh);
		m_bCrosshairEye[0] = m_bCrosshairEye[1] = LTFALSE;
		return;
	}

	// The crosshair images are drawn as textured quads at the headset's resolution, point sampled,
	// so each image pixel would show as a block: CrosshairSmoothing draws smoothed copies.
	g_pLTClient->StartOptimized2D();
	for(int nEye = 0; nEye < 2; nEye++)
	{
		if(m_bCrosshairEye[nEye])
			pCrosshair->DrawCrosshairAt(m_nCrosshairX[nEye], m_nCrosshairY[nEye], m_fCrosshairScaleX[nEye], m_fCrosshairScaleY[nEye],
										m_bSmoothCrosshair);
		m_bCrosshairEye[nEye] = LTFALSE;
	}
	g_pLTClient->EndOptimized2D();
}

// ----------------------------------------------------------------------- //

LTBOOL VRMgr::HidesCursor() const
{
	return m_pApi && g_vtVREnable.GetFloat() != 0.0f;
}

LTBOOL VRMgr::StopsWeaponIdle() const
{
	return m_pApi && g_vtVREnable.GetFloat() != 0.0f && !m_bWeaponIdleAnims;
}

// ----------------------------------------------------------------------- //

void VRMgr::PressKey(int nKey)
{
	g_pGameClientShell->OnKeyDown(nKey, 0);
	g_pGameClientShell->OnKeyUp(nKey);
}

// ----------------------------------------------------------------------- //
// Menus: either stick moves between options (up/down) and changes sliders and choices
// (left/right); menu select is Enter, menu back and pause are Escape (back).

void VRMgr::UpdateMenus()
{
	Avp2XrInput in;
	unsigned int nButtons = 0;
	LTFLOAT x = 0.0f, y = 0.0f;
	if(m_pApi->GetInput(&in))
	{
		nButtons = in.buttons;
		if(in.moveValid)
		{
			x = in.move[0];
			y = in.move[1];
		}
		if(in.turnValid && in.turn * in.turn + in.turnY * in.turnY > x * x + y * y)
		{
			x = in.turn;
			y = in.turnY;
		}
	}

	// Buttons already held when the menus came up (the pause button, say) aren't presses
	if(!m_bMenuInput)
	{
		m_bMenuInput = LTTRUE;
		m_nMenuButtons = nButtons;
		m_nMenuKey = 0;
	}
	unsigned int nPressed = nButtons & ~m_nMenuButtons;
	m_nMenuButtons = nButtons;

	if(nPressed & AVP2XR_BTN_MENUSELECT)
		PressKey(VK_RETURN);
	if(nPressed & (AVP2XR_BTN_MENUBACK | AVP2XR_BTN_PAUSE))
		PressKey(g_pClientButeMgr->GetEscapeKey());

	// The stick's main direction as an arrow key, held until the stick comes back
	LTFLOAT ax = (LTFLOAT)fabs(x), ay = (LTFLOAT)fabs(y);
	int nKey = 0;
	if((ax > ay ? ax : ay) > (m_nMenuKey ? VR_MENU_RELEASE : VR_MENU_PRESS))
		nKey = (ay >= ax) ? (y > 0.0f ? VK_UP : VK_DOWN) : (x > 0.0f ? VK_RIGHT : VK_LEFT);

	uint32 nNow = GetTickCount();
	if(nKey != m_nMenuKey)
	{
		if(m_nMenuKey)
			g_pGameClientShell->OnKeyUp(m_nMenuKey);
		m_nMenuKey = nKey;
		if(nKey)
		{
			g_pGameClientShell->OnKeyDown(nKey, 0);
			m_nMenuRepeatTime = nNow + VR_MENU_REPEAT_DELAY_MS;
		}
	}
	else if(nKey && (int32)(nNow - m_nMenuRepeatTime) >= 0)
	{
		g_pGameClientShell->OnKeyDown(nKey, 1);
		m_nMenuRepeatTime = nNow + VR_MENU_REPEAT_MS;
	}
}

// ----------------------------------------------------------------------- //

void VRMgr::LeaveMenus()
{
	if(m_nMenuKey)
		g_pGameClientShell->OnKeyUp(m_nMenuKey);
	m_nMenuKey = 0;

	// Whatever is held (select, having picked "resume") mustn't act in the game as well
	m_nIgnoreButtons = m_nMenuButtons;
	m_bMenuInput = LTFALSE;
}

// ----------------------------------------------------------------------- //
// Controller buttons go to the game the same way keys do: press and release as
// OnCommandOn/OnCommandOff. Held buttons also feed the control flags (see PlayerMovement).

static const struct { unsigned int nButton; int nCommand; } s_ButtonCommands[] =
{
	{ AVP2XR_BTN_FIRE,		COMMAND_ID_FIRING },
	{ AVP2XR_BTN_ALTFIRE,	COMMAND_ID_ALT_FIRING },
	{ AVP2XR_BTN_JUMP,		COMMAND_ID_JUMP },
	{ AVP2XR_BTN_CROUCH,	COMMAND_ID_DUCK },
	{ AVP2XR_BTN_USE,		COMMAND_ID_ACTIVATE },
	{ AVP2XR_BTN_VISION,	COMMAND_ID_NEXT_VISIONMODE },
	{ AVP2XR_BTN_RELOAD,	COMMAND_ID_RELOAD },
	{ AVP2XR_BTN_TAUNT,		COMMAND_ID_TAUNT },
	{ AVP2XR_BTN_FLARE,		COMMAND_ID_FLARE },		// tossed on release, Marines only
	{ AVP2XR_BTN_POUNCE,	COMMAND_ID_POUNCE_JUMP },	// only characters that can pounce (Aliens)
	{ AVP2XR_BTN_SHOULDERLAMP,	COMMAND_ID_SHOULDERLAMP },	// toggles on press, humans (Marines) only
};

void VRMgr::UpdateButtons()
{
	// Everything counts as released once controller input stops (menus, flat screen). Buttons
	// held when the menus closed count once they've been let go.
	unsigned int nNow = m_bLastFrameInput ? m_Input.buttons : 0;
	m_nIgnoreButtons &= nNow;
	nNow &= ~m_nIgnoreButtons;
	unsigned int nChanged = nNow ^ m_nButtons;
	m_nButtons = nNow;

	// Cloak is handled by the (retail) game server from the key bound to it, which the engine sends
	// itself, so the button presses that key: C (scancode 0x2E), as in the default controls
	if(nChanged & AVP2XR_BTN_CLOAK)
		SendKey(VR_CLOAK_SCANCODE, (nNow & AVP2XR_BTN_CLOAK) != 0);

	// Zoom (the Predator's): next level on each press, back to none after the last
	if((nChanged & nNow & AVP2XR_BTN_ZOOM) && g_pGameClientShell->GetInterfaceMgr()->GetGameState() == GS_PLAYING)
		g_pGameClientShell->GetPlayerStats()->CycleZoom();

	// The T item likewise
	if(nChanged & AVP2XR_BTN_TORCHSIFT)
		SendKey(VR_TORCHSIFT_SCANCODE, (nNow & AVP2XR_BTN_TORCHSIFT) != 0);

	// The H item likewise
	if(nChanged & AVP2XR_BTN_HACK)
		SendKey(VR_HACK_SCANCODE, (nNow & AVP2XR_BTN_HACK) != 0);

	// Medicomp likewise (G), only pressed for a Predator; a release is always passed on
	if(nChanged & AVP2XR_BTN_MEDICOMP)
	{
		LTBOOL bDown = (nNow & AVP2XR_BTN_MEDICOMP) != 0;
		if(!bDown || IsPredator(g_pGameClientShell->GetPlayerStats()->GetButeSet()))
			SendKey(VR_MEDICOMP_SCANCODE, bDown);
	}

	// Disc retrieve likewise (F), but only pressed for a Predator; a release is always passed on
	if(nChanged & AVP2XR_BTN_DISCRETRIEVE)
	{
		LTBOOL bDown = (nNow & AVP2XR_BTN_DISCRETRIEVE) != 0;
		if(!bDown || IsPredator(g_pGameClientShell->GetPlayerStats()->GetButeSet()))
			SendKey(VR_DISCRETRIEVE_SCANCODE, bDown);
	}

	// The pause button opens the menu like Escape
	if((nChanged & nNow & AVP2XR_BTN_PAUSE) && g_pGameClientShell->GetInterfaceMgr()->GetGameState() == GS_PLAYING)
		PressKey(g_pClientButeMgr->GetEscapeKey());

	// Objectives toggle: a press shows them (as Tab does while held), the next hides them. A
	// toggle, so a quick press works (a short press of a long-press input only lasts a moment).
	// Off again on leaving play (the menus).
	LTBOOL bPlaying = g_pGameClientShell->GetInterfaceMgr()->GetGameState() == GS_PLAYING;
	LTBOOL bObjectives = m_bObjectivesOn;
	if((nChanged & nNow & AVP2XR_BTN_OBJECTIVES) && bPlaying)
		bObjectives = !bObjectives;
	if(!bPlaying)
		bObjectives = LTFALSE;
	if(bObjectives != m_bObjectivesOn)
	{
		m_bObjectivesOn = bObjectives;
		if(bObjectives)
			g_pGameClientShell->OnCommandOn(COMMAND_ID_SCOREDISPLAY);
		else
			g_pGameClientShell->OnCommandOff(COMMAND_ID_SCOREDISPLAY);
	}

	for(int i = 0; i < sizeof(s_ButtonCommands) / sizeof(s_ButtonCommands[0]); i++)
	{
		if(!(nChanged & s_ButtonCommands[i].nButton))
			continue;
		if(nNow & s_ButtonCommands[i].nButton)
			g_pGameClientShell->OnCommandOn(s_ButtonCommands[i].nCommand);
		else
			g_pGameClientShell->OnCommandOff(s_ButtonCommands[i].nCommand);
	}

	// The NextWeapon/PrevWeapon buttons cycle weapons, one per press. By default avp2xr.ini has
	// them on the right stick up/down (the proxy turns stick directions into presses).
	int nCycle = 0;
	if(nChanged & nNow & AVP2XR_BTN_NEXTWEAPON)
		nCycle = COMMAND_ID_NEXT_WEAPON;
	else if(nChanged & nNow & AVP2XR_BTN_PREVWEAPON)
		nCycle = COMMAND_ID_PREV_WEAPON;
	if(nCycle)
	{
		g_pGameClientShell->OnCommandOn(nCycle);
		g_pGameClientShell->OnCommandOff(nCycle);
		m_nCycleSelectTime = GetTickCount() + VR_CYCLE_SELECT_MS;
		if(!m_nCycleSelectTime)
			m_nCycleSelectTime = 1;
	}

	// Select the highlighted weapon the way the fire button does, unless the chooser has
	// already closed (fired, timed out, player died)
	if(m_nCycleSelectTime && (int32)(GetTickCount() - m_nCycleSelectTime) >= 0)
	{
		m_nCycleSelectTime = 0;
		CInterfaceMgr *pInterface = g_pGameClientShell->GetInterfaceMgr();
		if(pInterface->IsChoosingWeapon())
			pInterface->OnCommandOn(COMMAND_ID_FIRING);
	}
}

// ----------------------------------------------------------------------- //

LTBOOL VRMgr::BeginFrame()
{
	m_bStereo = LTFALSE;
	m_bRendered = LTFALSE;
	m_bSubmitted = LTFALSE;

	if(!g_vtVREnable.GetFloat() || !FindApi())
		return LTFALSE;

	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	g_pLTClient->GetSurfaceDims(hScreen, &m_nScreenWidth, &m_nScreenHeight);
	if(m_nScreenWidth < 2 || m_nScreenHeight < 1)
		return LTFALSE;

	m_bStereo = m_pApi->BeginStereoFrame(m_Views) ? LTTRUE : LTFALSE;

	// Re-anchor the view after any flat period (level load, menus), so each level starts
	// facing the way the game faces the player.
	if(m_bStereo && !m_bLastFrameStereo)
	{
		m_bAnchorValid = LTFALSE;

		// d3d.ren locks a corner of the back buffer at every flip (LockOnFlip, default 1) so the
		// CPU can't run ahead of the GPU. Under dgVoodoo that's a full GPU wait and readback,
		// about 5 ms per flip, twice per stereo frame. xrWaitFrame paces the frames instead.
		SetConsoleFloat("LockOnFlip", 0.0f);
	}

	m_bInput = LTFALSE;
	m_bPlayerView = LTFALSE;
	if(m_bStereo)
	{
		m_bInput = m_pApi->GetInput(&m_Input) ? LTTRUE : LTFALSE;

		// Holding the Meta/system button makes the runtime recenter; re-anchor with it so the
		// view faces where the player aims and the eyes go back to the camera height.
		if(m_Input.recenterCount != m_nRecenterCount)
		{
			m_nRecenterCount = m_Input.recenterCount;
			m_bAnchorValid = LTFALSE;
		}

		// Right stick left/right turns the view (avp2xr.ini TurnMode): smoothly, faster the further
		// it's pushed, or in snaps, one per push. The body follows the head, so it turns too. Only
		// while left/right is the stick's main direction, so weapon cycling (up/down) doesn't turn.
		LTFLOAT fSnap = g_vtVRSnapTurn.GetFloat();
		if(m_Input.turnValid && m_bAnchorValid)
		{
			LTFLOAT x = m_Input.turn;
			LTBOOL bSideways = fabs(x) > fabs(m_Input.turnY);
			if(m_bSmoothTurn)
			{
				LTFLOAT fDeflect = (LTFLOAT)fabs(x);
				if(bSideways && fDeflect > VR_STICK_DEADZONE)
				{
					LTFLOAT fFrameTime = g_pLTClient->GetFrameTime();
					if(fFrameTime > 0.1f)
						fFrameTime = 0.1f;
					LTFLOAT fAmount = (fDeflect - VR_STICK_DEADZONE) / (1.0f - VR_STICK_DEADZONE);
					if(fAmount > 1.0f)
						fAmount = 1.0f;
					m_fAnchorYaw += MATH_DEGREES_TO_RADIANS(m_fSmoothTurnSpeed) * fAmount * fFrameTime * (x > 0.0f ? 1.0f : -1.0f);
				}
			}
			else if(fSnap > 0.0f)
			{
				if(m_bSnapArmed && (LTFLOAT)fabs(x) > VR_SNAP_PRESS && bSideways)
				{
					m_fAnchorYaw += MATH_DEGREES_TO_RADIANS(x > 0.0f ? fSnap : -fSnap);
					m_bSnapArmed = LTFALSE;
				}
				else if(x < VR_SNAP_RELEASE && x > -VR_SNAP_RELEASE)
					m_bSnapArmed = LTTRUE;
			}
		}
	}
	m_bAimWorld = LTFALSE;
	m_bOffWorld = LTFALSE;
	m_bMuzzleWorld = LTFALSE;
	m_bCrosshairHit = LTFALSE;

	// The left arm's move is only right while the stereo frames keep working it out
	if(!m_bStereo || (int32)(GetTickCount() - m_nArmTime) > 250)
		m_bArmActive = LTFALSE;

	return m_bStereo;
}

// ----------------------------------------------------------------------- //

void VRMgr::GetEyeCamera(int nEye, HCAMERA hCamera, const LTVector &vPos, const LTRotation &rRot,
						 LTVector &vEyePos, LTRotation &rEyeRot,
						 LTFLOAT &fFOVX, LTFLOAT &fFOVY, LTRect &rRect)
{
	const Avp2XrView &view = m_Views[nEye];
	LTRotation rHead = FromXr(view.orientation);

	LTFLOAT fYaw = YawOf(rRot);

	if(hCamera == m_hPlayerCamera)
	{
		m_bPlayerView = LTTRUE;

		// The player's own view turns only with the head: the aiming controller (or the mouse)
		// turns the body, not the view. The view is anchored to the player's facing when stereo
		// started.
		// The head position at that moment becomes the reference too, so the eyes start at the
		// game's own camera height and only real head movement from there moves the view.
		if(!m_bAnchorValid)
		{
			m_fAnchorYaw = fYaw;
			for(int i = 0; i < 3; i++)
				m_fHeadRef[i] = (m_Views[0].position[i] + m_Views[1].position[i]) * 0.5f;
			m_bAnchorValid = LTTRUE;
		}
		fYaw = m_fAnchorYaw;
	}

	LTRotation rYaw = YawRotation(fYaw);
	if(hCamera == m_hPlayerCamera)
		rEyeRot = rYaw * rHead;
	else
		rEyeRot = rRot * rHead;	// other cameras (cutscenes) keep their scripted rotation

	// Head movement is applied in the yaw-only frame, so leaning forward moves you forward
	// whatever the pitch. For the player's view the camera position is the reference head
	// position; other cameras treat the tracking origin as the camera position.
	const float *ref = (hCamera == m_hPlayerCamera) ? m_fHeadRef : s_fNoRef;
	LTFLOAT fScale = g_vtVRWorldScale.GetFloat();
	vEyePos = vPos + Rotate(rYaw, FromXrPos(view.position, ref) * fScale);

	// The aiming controller in the world, for drawing the weapon there
	if(nEye == 0 && hCamera == m_hPlayerCamera && m_bInput && m_Input.aimValid)
	{
		m_vAimPos = vPos + Rotate(rYaw, FromXrPos(m_Input.aimPosition, ref) * fScale);
		m_bGripWorld = m_Input.gripValid;
		if(m_bGripWorld)
			m_vGripPos = vPos + Rotate(rYaw, FromXrPos(m_Input.gripPosition, ref) * fScale);
		m_rAimRot = rYaw * FromXr(m_Input.aimOrientation) * m_rGunAngleCur;
		m_bAimWorld = LTTRUE;

		// The off hand, for the model's left arm. Only its grip pose is known; the right hand's
		// model frame is its aim pose, so the left one is its grip pose turned by the right
		// controller's grip-to-aim angle, mirrored.
		if(m_Input.gripValid)
			m_rGripToAim = FromXr(m_Input.gripOrientation).Conjugate() * FromXr(m_Input.aimOrientation);
		m_bOffWorld = m_Input.offValid;
		if(m_bOffWorld)
		{
			m_vOffPos = vPos + Rotate(rYaw, FromXrPos(m_Input.offPosition, ref) * fScale);
			m_rOffRot = rYaw * FromXr(m_Input.offOrientation) * MirrorX(m_rGripToAim);
		}

		// Two-handed aiming: with the off hand in front of the gun hand, the gun points from
		// the gun hand to the off hand, keeping the gun hand's roll.
		m_bTwoHanded = m_bTwoHanded && m_Input.offValid;
		if(m_Input.offValid && g_vtVRTwoHanded.GetFloat() != 0.0f)
		{
			LTVector vOff = vPos + Rotate(rYaw, FromXrPos(m_Input.offPosition, ref) * fScale);
			LTVector vToOff = vOff - m_vAimPos;
			LTFLOAT fLen = vToOff.Mag();
			LTFLOAT fMetres = fLen / fScale;
			LTVector vR, vU, vF;
			LTRotation rAim = m_rAimRot;
			g_pLTClient->GetMathLT()->GetRotationVectors(rAim, vR, vU, vF);
			LTFLOAT fCos = (fLen > 0.0001f) ? vToOff.Dot(vF) / fLen : -1.0f;

			if(m_bTwoHanded)
				m_bTwoHanded = fMetres > VR_TWOHAND_RELEASE_MIN && fMetres < VR_TWOHAND_RELEASE_MAX &&
							   fCos > (LTFLOAT)cos(MATH_DEGREES_TO_RADIANS(VR_TWOHAND_RELEASE_ANGLE));
			else
				m_bTwoHanded = fMetres > VR_TWOHAND_MIN && fMetres < VR_TWOHAND_MAX &&
							   fCos > (LTFLOAT)cos(MATH_DEGREES_TO_RADIANS(VR_TWOHAND_ANGLE));

			if(m_bTwoHanded)
			{
				LTVector vForward = vToOff / fLen;
				LTVector vUp = vU - vForward * vU.Dot(vForward);
				if(vUp.MagSqr() > 0.0001f)
				{
					vUp.Norm();
					m_rAimRot = QuatFromBasis(CrossStd(vUp, vForward), vUp, vForward);
				}
			}
		}
		else
			m_bTwoHanded = LTFALSE;
	}

	// The engine only does symmetric frustums, so cover the eye's (asymmetric) FOV with a
	// symmetric one and tell OpenXR that's what was rendered.
	LTFLOAT fHalfX = -view.fov[0] > view.fov[1] ? -view.fov[0] : view.fov[1];
	LTFLOAT fHalfY = view.fov[2] > -view.fov[3] ? view.fov[2] : -view.fov[3];
	fFOVX = fHalfX * 2.0f;
	fFOVY = fHalfY * 2.0f;

	LTFLOAT fTanX = (LTFLOAT)tan(fHalfX), fTanY = (LTFLOAT)tan(fHalfY);
	if(nEye == 0 || fTanX > m_fTanHalfX) m_fTanHalfX = fTanX;
	if(nEye == 0 || fTanY > m_fTanHalfY) m_fTanHalfY = fTanY;

	int nHalf = (int)m_nScreenWidth / 2;
	rRect.left = nEye ? nHalf : 0;
	rRect.top = 0;
	rRect.right = nEye ? (int)m_nScreenWidth : nHalf;
	rRect.bottom = (int)m_nScreenHeight;

	// SetEyeInset: a smaller rect about the middle, and the FOV narrowed by the same fraction of its
	// tangent, so what's shown is the middle of the view in its place (black around it)
	if(m_fEyeInsetX > 0.0f || m_fEyeInsetY > 0.0f)
	{
		LTFLOAT fX = m_fEyeInsetX < 0.98f ? m_fEyeInsetX : 0.98f;
		LTFLOAT fY = m_fEyeInsetY < 0.98f ? m_fEyeInsetY : 0.98f;
		int nW = rRect.right - rRect.left, nH = rRect.bottom - rRect.top;
		int nDX = (int)(nW * fX * 0.5f), nDY = (int)(nH * fY * 0.5f);
		rRect.left += nDX;
		rRect.right -= nDX;
		rRect.top += nDY;
		rRect.bottom -= nDY;
		if(nW > 0 && nH > 0)
		{
			fHalfX = (LTFLOAT)atan(tan(fHalfX) * (rRect.right - rRect.left) / nW);
			fHalfY = (LTFLOAT)atan(tan(fHalfY) * (rRect.bottom - rRect.top) / nH);
			fFOVX = fHalfX * 2.0f;
			fFOVY = fHalfY * 2.0f;
		}
	}

	Avp2XrEyeSubmit &submit = m_Submit[nEye];
	submit.fov[0] = -fHalfX;
	submit.fov[1] = fHalfX;
	submit.fov[2] = fHalfY;
	submit.fov[3] = -fHalfY;
	submit.rect[0] = (float)rRect.left / m_nScreenWidth;
	submit.rect[1] = (float)rRect.top / m_nScreenHeight;
	submit.rect[2] = (float)(rRect.right - rRect.left) / m_nScreenWidth;
	submit.rect[3] = (float)(rRect.bottom - rRect.top) / m_nScreenHeight;

	// Zoomed: rendered with a narrower FOV than it's shown with (OpenXR gets the unzoomed one above)
	m_fShownFOVX = fFOVX;
	m_fShownFOVY = fFOVY;
	if(m_fEyeZoom < 0.999f && m_fEyeZoom > 0.01f && m_fZoomStrength > 0.0f)
	{
		LTFLOAT fZoom = (LTFLOAT)pow(m_fEyeZoom, m_fZoomStrength);
		fHalfX = (LTFLOAT)atan(tan(fHalfX) * fZoom);
		fHalfY = (LTFLOAT)atan(tan(fHalfY) * fZoom);
		fFOVX = fHalfX * 2.0f;
		fFOVY = fHalfY * 2.0f;
	}
}

// ----------------------------------------------------------------------- //

void VRMgr::SubmitEyes(LTBOOL bHudFollows)
{
	if(!IsStereoRendered() || m_bSubmitted)
		return;

	m_pApi->SubmitStereo(m_Submit, bHudFollows ? AVP2XR_HUD_FOLLOWS : 0);
	m_bSubmitted = LTTRUE;
}

// ----------------------------------------------------------------------- //

void VRMgr::EndFrame()
{
	SubmitEyes(LTFALSE);

	m_bLastFrameStereo = IsStereoRendered();

	// Stereo frame rate, every couple of seconds: below the headset's rate the runtime
	// reprojects, which makes close things like the hands wobble
	static uint32 s_nFrames = 0, s_nStart = 0;
	uint32 nNow = GetTickCount();
	if(m_bLastFrameStereo)
	{
		if(!s_nFrames++)
			s_nStart = nNow;
		else if(nNow - s_nStart >= 2000)
		{
			VRLog("Stereo frame rate %.1f fps", (s_nFrames - 1) * 1000.0f / (nNow - s_nStart));
			s_nFrames = 0;
		}
	}
	else
		s_nFrames = 0;
	m_bLastFrameInput = m_bLastFrameStereo && m_bInput && m_bAnchorValid;
	m_bLastFramePlayerView = m_bLastFrameStereo && m_bPlayerView && m_bAnchorValid;
	m_bLastFrameAimWorld = m_bLastFrameStereo && m_bAimWorld;
	m_bLastFrameMuzzle = m_bLastFrameStereo && m_bMuzzleWorld;
	if(m_bLastFrameStereo && m_bAnchorValid)
	{
		m_rHeadWorld = YawRotation(m_fAnchorYaw) * FromXr(m_Views[0].orientation);
		m_fHeadYaw = YawOf(m_rHeadWorld);
	}

	m_bStereo = LTFALSE;
	m_bRendered = LTFALSE;
	m_bSubmitted = LTFALSE;
}

// ----------------------------------------------------------------------- //

LTBOOL VRMgr::GetStereoFOVTangents(LTFLOAT &fTanX, LTFLOAT &fTanY) const
{
	if(!m_bLastFrameStereo)
		return LTFALSE;

	fTanX = m_fTanHalfX;
	fTanY = m_fTanHalfY;
	return LTTRUE;
}

// ----------------------------------------------------------------------- //

LTBOOL VRMgr::GetMoveDirection(LTVector &vDir) const
{
	if(!m_bLastFrameInput || !m_Input.moveValid)
		return LTFALSE;

	LTFLOAT x = m_Input.move[0], y = m_Input.move[1];
	LTFLOAT fMag = (LTFLOAT)sqrt(x * x + y * y);
	if(fMag < VR_STICK_DEADZONE)
		return LTFALSE;

	LTFLOAT fLength = (fMag - VR_STICK_DEADZONE) / (1.0f - VR_STICK_DEADZONE);
	if(fLength > 1.0f) fLength = 1.0f;

	// Stick up = where the head faces, stick right = the head's right
	LTFLOAT fSin = (LTFLOAT)sin(m_fHeadYaw), fCos = (LTFLOAT)cos(m_fHeadYaw);
	LTVector vForward(fSin, 0.0f, fCos), vRight(fCos, 0.0f, -fSin);
	vDir = (vRight * x + vForward * y) * (fLength / fMag);
	return LTTRUE;
}

LTBOOL VRMgr::GetMoveStick(LTFLOAT &fRight, LTFLOAT &fForward) const
{
	if(!m_bLastFrameInput || !m_Input.moveValid)
		return LTFALSE;

	LTFLOAT x = m_Input.move[0], y = m_Input.move[1];
	LTFLOAT fMag = (LTFLOAT)sqrt(x * x + y * y);
	if(fMag < VR_STICK_DEADZONE)
		return LTFALSE;

	LTFLOAT fLength = (fMag - VR_STICK_DEADZONE) / (1.0f - VR_STICK_DEADZONE);
	if(fLength > 1.0f) fLength = 1.0f;
	fRight = x * fLength / fMag;
	fForward = y * fLength / fMag;
	return LTTRUE;
}

// ----------------------------------------------------------------------- //

LTBOOL VRMgr::GetAimDirection(LTVector &vDir) const
{
	if(!m_bLastFrameInput || !m_Input.aimValid)
		return LTFALSE;

	vDir = Rotate(YawRotation(m_fAnchorYaw) * FromXr(m_Input.aimOrientation) * m_rGunAngleCur, LTVector(0.0f, 0.0f, 1.0f));
	return LTTRUE;
}

// ----------------------------------------------------------------------- //

LTBOOL VRMgr::GetHeadDirection(LTVector &vDir) const
{
	if(!m_bLastFramePlayerView)
		return LTFALSE;

	vDir = Rotate(m_rHeadWorld, LTVector(0.0f, 0.0f, 1.0f));
	return LTTRUE;
}

// ----------------------------------------------------------------------- //

LTBOOL VRMgr::GetAimPose(LTVector &vPos, LTRotation &rRot) const
{
	if(!m_bLastFrameAimWorld)
		return LTFALSE;

	vPos = m_bLastFrameMuzzle ? m_vShotPos : m_vAimPos;
	rRot = m_rAimRot;
	return LTTRUE;
}

// ----------------------------------------------------------------------- //

LTBOOL VRMgr::GetMuzzlePose(LTVector &vPos, LTRotation &rRot) const
{
	if(!m_bLastFrameMuzzle)
		return LTFALSE;

	vPos = m_vMuzzlePos;
	rRot = m_rAimRot;
	return LTTRUE;
}

// ----------------------------------------------------------------------- //

void VRMgr::PlaceWeapon()
{
	if(!m_bAimWorld || m_bWeaponMoved)
	{
		static uint32 s_nNextSkipLog = 0;
		if(!m_bAimWorld && (int32)(GetTickCount() - s_nNextSkipLog) >= 0)
		{
			s_nNextSkipLog = GetTickCount() + 2000;
			VRLog("Weapon not placed: no aim pose (input %d, aim tracked %d)", m_bInput, m_Input.aimValid);
		}
		return;
	}

	CWeaponModel *pWeapon = g_pGameClientShell->GetWeaponModel();
	HOBJECT hWeapon = pWeapon ? pWeapon->GetHandle() : LTNULL;
	if(!hWeapon)
		return;

	// Remember the game's own (view-relative, really-close) placement to put back afterwards
	g_pLTClient->GetObjectPos(hWeapon, &m_vWeaponPos);
	g_pLTClient->GetObjectRotation(hWeapon, &m_rWeaponRot);
	g_pLTClient->GetObjectScale(hWeapon, &m_vWeaponScale);
	m_nWeaponFlags = g_pLTClient->GetObjectFlags(hWeapon);
	m_bWeaponMoved = LTTRUE;
	m_bWeaponView = g_vtVRGunMode.GetFloat() != 0.0f && (m_nWeaponFlags & FLAG_REALLYCLOSE);

	// The model's origin is the flat-screen eye point, with the arms reaching forward from it,
	// so placing the origin at the controller puts the hand an arm's length ahead of the real
	// one. Instead the model's right palm (between its wrist and middle knuckle, tracked every
	// frame so it stays put through the animations) goes on the controller. Models without hand
	// nodes use VRGrip. VRGunOffset (world units, in the controller's frame: right, up, forward)
	// and VRGunScale fine-tune the fit.
	// The palm is only followed while the weapon is idle (and until it first is, e.g. while it's
	// being raised). Holding it through firing and reloading lets those animations move the hand
	// and weapon, instead of pinning the palm and leaving the knife to spin around it.
	LTFLOAT fScale = g_vtVRGunScale.GetFloat();
	LTVector vWrist, vKnuckle, vPalm;
	const char *szGrip = "default";
	LTBOOL bWrist = FindModelNode(hWeapon, s_szWristNodes, 4, vWrist);
	LTBOOL bKnuckle = FindModelNode(hWeapon, s_szKnuckleNodes, 4, vKnuckle);
	if(pWeapon->GetWeapon() != m_pGripWeapon)
	{
		m_pGripWeapon = pWeapon->GetWeapon();
		m_bGripHeld = LTFALSE;

		// avp2xr.ini [Gun] <weapon> = forward up right [pitch yaw roll]: this weapon's own place in
		// the hand (cm, degrees), in place of the Gun* settings. Read now, so editing the ini (and
		// running update_ini.bat) takes effect at the next change of weapon.
		m_vGunOffsetCur = LTVector(g_vtVRGunOffsetX.GetFloat(), g_vtVRGunOffsetY.GetFloat(), g_vtVRGunOffsetZ.GetFloat());
		m_rGunAngleCur = m_rGunAngle;
		char szValue[128] = "";
		const char *szName = m_pGripWeapon ? m_pGripWeapon->szName : "";
		if(m_szIni[0] && szName[0])
			GetPrivateProfileStringA("Gun", szName, "", szValue, sizeof(szValue), m_szIni);
		float f[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		int nValues = szValue[0] ? sscanf(szValue, "%f %f %f %f %f %f", &f[0], &f[1], &f[2], &f[3], &f[4], &f[5]) : 0;
		if(nValues >= 1)
		{
			LTFLOAT fUnitsPerCm = g_vtVRWorldScale.GetFloat() / 100.0f;
			m_vGunOffsetCur = LTVector(f[2], f[1], f[0]) * fUnitsPerCm;
		}
		if(nValues >= 4)
			m_rGunAngleCur = GunAngleRotation(f[3], f[4], f[5]);
		VRLog("Gun for %s: [Gun] %s=%s", szName[0] ? szName : "?", szName[0] ? szName : "?",
			nValues >= 1 ? szValue : "(not set: the Gun* settings)");
	}
	if(bWrist && bKnuckle)
	{
		vPalm = (vWrist + vKnuckle) * 0.5f;
		szGrip = "palm";
	}
	else if(bWrist)
	{
		vPalm = vWrist;
		szGrip = "wrist";
	}
	else
		vPalm = LTVector(g_vtVRGripX.GetFloat(), g_vtVRGripY.GetFloat(), g_vtVRGripZ.GetFloat());
	if(bWrist)
	{
		if(pWeapon->GetState() == W_IDLE || !m_bGripHeld)
		{
			m_vGripPalm = vPalm;
			m_bGripHeld = pWeapon->GetState() == W_IDLE;
		}
		else
			vPalm = m_vGripPalm;
	}

	// The palm goes where the real one is (the grip pose), and the weapon points along the aim
	// (the aim pose, or hand to hand when two-handed) like the shots.
	LTVector vHand = m_bGripWorld ? m_vGripPos :
		m_vAimPos - Rotate(m_rAimRot, LTVector(0.0f, 0.0f, VR_AIM_TO_PALM * g_vtVRWorldScale.GetFloat()));
	LTVector vOffset = vPalm * fScale;
	vOffset -= m_vGunOffsetCur;
	LTVector vOrigin = vHand - Rotate(m_rAimRot, vOffset);

	// Log the grip whenever the weapon changes, for tuning
	static WEAPON *s_pLastWeapon = LTNULL;
	if(pWeapon->GetWeapon() != s_pLastWeapon)
	{
		s_pLastWeapon = pWeapon->GetWeapon();
		VRLog("Weapon %s: grip %s (%.1f %.1f %.1f), wrist %d (%.1f %.1f %.1f), knuckle %d (%.1f %.1f %.1f)",
			s_pLastWeapon ? s_pLastWeapon->szName : "?", szGrip, vPalm.x, vPalm.y, vPalm.z,
			bWrist, bWrist ? vWrist.x : 0.0f, bWrist ? vWrist.y : 0.0f, bWrist ? vWrist.z : 0.0f,
			bKnuckle, bKnuckle ? vKnuckle.x : 0.0f, bKnuckle ? vKnuckle.y : 0.0f, bKnuckle ? vKnuckle.z : 0.0f);
	}

	m_vWeaponOrigin = vOrigin;

	// The muzzle flash belongs at the muzzle: the model's muzzle socket, or else part way along
	// the line from the flat-screen eye (at minus the game's placement, in model space) to the
	// game's flash position (the muzzle offset from the model's origin)
	LTVector vMuzzle;
	LTVector vFlatEye = -m_vWeaponPos;
	LTVector vFlatFlash = pWeapon->GetMuzzleOffset();
	LTBOOL bMuzzle = FindModelSocket(hWeapon, s_szMuzzleSockets, 3, vMuzzle);
	if(!bMuzzle)
		vMuzzle = vFlatEye + (vFlatFlash - vFlatEye) * g_vtVRFlashDepth.GetFloat();
	m_vMuzzlePos = vOrigin + Rotate(m_rAimRot, vMuzzle * fScale);
	m_bMuzzleWorld = LTTRUE;

	// Shots (and the crosshair) come out of the muzzle along the aim, so they follow the barrel
	// wherever the gun sits in the hand. Weapons without a muzzle (melee: no socket and no flash
	// position) fire from the controller.
	m_vShotPos = (bMuzzle || vFlatFlash.MagSqr() > 0.01f) ? m_vMuzzlePos : m_vAimPos;

	// Where the shots go, for the crosshair
	m_bCrosshairHit = LTFALSE;
	CCrosshairMgr *pCrosshair = g_pGameClientShell->GetInterfaceMgr()->GetCrosshairMgr();
	if(m_bShowCrosshair && pCrosshair && pCrosshair->WantsVRCrosshair() && WeaponHasCrosshair(pWeapon->GetWeapon()))
	{
		LTVector vAimDir = Rotate(m_rAimRot, LTVector(0.0f, 0.0f, 1.0f));
		IntersectQuery iq;
		IntersectInfo ii;
		iq.m_From = m_vShotPos;
		iq.m_To = m_vShotPos + vAimDir * VR_CROSSHAIR_RANGE;
		iq.m_Flags = INTERSECT_OBJECTS | IGNORE_NONSOLID;
		iq.m_FilterFn = CrosshairFilterFn;
		iq.m_pUserData = hWeapon;
		m_vCrosshairPos = g_pLTClient->IntersectSegment(&iq, &ii) ? ii.m_Point : iq.m_To;
		m_bCrosshairHit = LTTRUE;
	}

	LTFLOAT fFlatDist = (vFlatFlash - vFlatEye).Mag();
	LTFLOAT fFlashScale = g_vtVRFlashScale.GetFloat();
	if(fFlatDist > 0.01f)
		fFlashScale *= (vMuzzle - vFlatEye).Mag() / fFlatDist;

	CMuzzleFlashFX *pFlash = pWeapon->GetMuzzleFlash();
	if(pFlash)
	{
		pFlash->PlaceLight(m_vMuzzlePos);
		if(m_bWeaponView)
			pFlash->BeginVRScale(fFlashScale);
	}
	m_fWeaponScale = (fScale > 0.01f) ? fScale : 0.01f;

	PlaceLeftArm(hWeapon, pWeapon->GetWeapon(), pWeapon->GetState());

	// VRGunMode 0: a normal world object, scaled up. The engine lights a scaled-up model badly
	// (black unless turned to the light) and it looks stretched, so by default the weapon stays
	// a really-close view model at its own size and PlaceWeaponForEye puts it in each eye's view.
	// The game sets the view-model projection with ExtraFOVX/YOffset for the flat camera;
	// PlaceWeaponForEye replaces them for each eye and RestoreWeapon puts them back.
	if(m_bWeaponView)
	{
		m_fExtraFOVX = GetConsoleFloat("ExtraFOVXOffset");
		m_fExtraFOVY = GetConsoleFloat("ExtraFOVYOffset");
	}
	else
	{
		g_pLTClient->SetObjectFlags(hWeapon, m_nWeaponFlags & ~FLAG_REALLYCLOSE);
		LTVector vScale = m_vWeaponScale * fScale;
		g_pLTClient->SetObjectScale(hWeapon, &vScale);
		g_pLTClient->SetObjectPos(hWeapon, &vOrigin, LTTRUE);
		g_pLTClient->SetObjectRotation(hWeapon, &m_rAimRot);
	}

	// Diagnostics, every couple of seconds: did the flag change stick, and where did it go?
	static uint32 s_nNextLog = 0;
	uint32 nNow = GetTickCount();
	if(!m_bWeaponView && (int32)(nNow - s_nNextLog) >= 0)
	{
		s_nNextLog = nNow + 2000;
		LTVector vCam, vGot, vGotScale;
		g_pLTClient->GetObjectPos(g_pGameClientShell->GetPlayerCameraObject(), &vCam);
		g_pLTClient->GetObjectPos(hWeapon, &vGot);
		g_pLTClient->GetObjectScale(hWeapon, &vGotScale);
		uint32 nNowFlags = g_pLTClient->GetObjectFlags(hWeapon);
		VRLog("Weapon %p: flags %08x -> %08x (reallyclose %s), game pos (%.1f %.1f %.1f) scale %.2f, "
			"placed (%.1f %.1f %.1f) scale %.2f, controller (%.1f %.1f %.1f), camera (%.1f %.1f %.1f), two-handed %d",
			hWeapon, m_nWeaponFlags, nNowFlags, (nNowFlags & FLAG_REALLYCLOSE) ? "STILL SET" : "cleared",
			m_vWeaponPos.x, m_vWeaponPos.y, m_vWeaponPos.z, m_vWeaponScale.x,
			vGot.x, vGot.y, vGot.z, vGotScale.x, m_vAimPos.x, m_vAimPos.y, m_vAimPos.z,
			vCam.x, vCam.y, vCam.z, m_bTwoHanded);
	}
}

// ----------------------------------------------------------------------- //
// A really-close model is drawn in the camera's own frame (the game leaves it at the weapon
// offset with no rotation). Seen from the eye, a model at 1/S the size and 1/S the distance
// looks exactly like one at full size and distance, so the native model goes at 1/S of the way
// to where PlaceWeapon wants it, in this eye's frame. It keeps the engine's own view-model
// lighting.
//
// Its projection has to match the eye's. d3d.ren scales a really-close model's screen position
// by tan(A / 2), where A = camera FOV + ExtraFOVOffset (degrees) + the model's FOV offset, while
// the world is scaled by 1 / tan(FOV / 2). So the view model's own FOV is 180 degrees - A (the
// flat game's 75 + 20 gives 85), and matching the eye's FOV takes an offset of 180 - 2 x FOV.

void VRMgr::PlaceWeaponForEye(const LTVector &vEyePos, const LTRotation &rEyeRot, LTFLOAT fFOVX, LTFLOAT fFOVY)
{
	if(!m_bWeaponMoved || !m_bWeaponView)
		return;

	// The view model's FOV is 180 - (camera FOV + offset); it's the FOV the eye is shown with, which
	// is the camera's unless zoomed (the weapon isn't magnified by a zoom, as on the flat screen)
	LTFLOAT fShownX = m_fShownFOVX > 0.0f ? m_fShownFOVX : fFOVX;
	LTFLOAT fShownY = m_fShownFOVY > 0.0f ? m_fShownFOVY : fFOVY;
	SetConsoleFloat("ExtraFOVXOffset", 180.0f - MATH_RADIANS_TO_DEGREES(fFOVX) - MATH_RADIANS_TO_DEGREES(fShownX));
	SetConsoleFloat("ExtraFOVYOffset", 180.0f - MATH_RADIANS_TO_DEGREES(fFOVY) - MATH_RADIANS_TO_DEGREES(fShownY));

	CWeaponModel *pWeapon = g_pGameClientShell->GetWeaponModel();
	HOBJECT hWeapon = pWeapon ? pWeapon->GetHandle() : LTNULL;
	if(!hWeapon)
		return;

	LTRotation rToEye = rEyeRot.Conjugate();
	LTVector vPos = Rotate(rToEye, m_vWeaponOrigin - vEyePos) / m_fWeaponScale;
	LTRotation rRot = rToEye * m_rAimRot;
	g_pLTClient->SetObjectPos(hWeapon, &vPos, LTTRUE);
	g_pLTClient->SetObjectRotation(hWeapon, &rRot);

	// The flash's really-close parts go at the muzzle the same way. The game puts them back at
	// its own place every frame it shows them.
	// So do the weapon's own fx at its sockets (the flamethrower's pilot light...)
	pWeapon->GetPVFXMgr()->PlaceForView(vEyePos, rEyeRot, m_fWeaponScale);

	CMuzzleFlashFX *pFlash = pWeapon->GetMuzzleFlash();
	if(pFlash)
		pFlash->PlaceInView(Rotate(rToEye, m_vMuzzlePos - vEyePos) / m_fWeaponScale, rRot);

	PlaceRailOverlay(vEyePos, rEyeRot);

	if(m_bArmCopyActive && m_hArmCopy)
	{
		LTVector vArmPos = Rotate(rToEye, m_vArmCopyOrigin - vEyePos) / m_fWeaponScale;
		LTRotation rArmRot = rToEye * m_rArmCopyRot;
		g_pLTClient->SetObjectPos(m_hArmCopy, &vArmPos, LTTRUE);
		g_pLTClient->SetObjectRotation(m_hArmCopy, &rArmRot);
	}

	static uint32 s_nNextLog = 0;
	uint32 nNow = GetTickCount();
	if((int32)(nNow - s_nNextLog) >= 0)
	{
		s_nNextLog = nNow + 2000;
		VRLog("Weapon view model: game pos (%.2f %.2f %.2f), eye pos (%.2f %.2f %.2f) scale 1/%.1f, two-handed %d",
			m_vWeaponPos.x, m_vWeaponPos.y, m_vWeaponPos.z, vPos.x, vPos.y, vPos.z, m_fWeaponScale, m_bTwoHanded);
	}
}

// ----------------------------------------------------------------------- //
// The model's left arm goes to the left controller the way the whole model goes to the right one:
// its palm (held from the idle pose, so reloads and attacks still move the arm) on the controller's
// grip pose, turned with the controller. That's a rigid move of the arm's root node in model
// space, done by a node control while the model is drawn; the rest of the arm follows its root.
//
// A model-space point p is drawn at O + R (S p) (O = m_vWeaponOrigin, R = m_rAimRot, S = the
// scale). Placed at the left hand instead it would be at G + L S (p - palm) (G = the left palm's
// place, L = m_rOffRot). So p moves to R^-1 (G - O) / S + R^-1 L (p - palm).

void VRMgr::PlaceLeftArm(HOBJECT hWeapon, WEAPON *pWeapon, int nState)
{
	// The nodes, the node control and this weapon's settings, whenever the model or weapon changes
	if(hWeapon != m_hArmModel || pWeapon != m_pArmWeapon)
	{
		m_hArmModel = hWeapon;
		m_pArmWeapon = pWeapon;
		m_bArmSeen = LTFALSE;
		m_bArmHeld = LTFALSE;
		m_bArmActive = LTFALSE;
		m_nArmForced = -1;
		const char *szRoot = "none";
		m_hArmRoot = FindNodeHandle(hWeapon, s_szLeftArmNodes, 4, &szRoot);
		int nHand = 0;
		for(int h = 0; h < 4; h++)
		{
			m_hArmHand[h] = FindNodeHandle(hWeapon, s_pszLeftHandNodes[h], 4, LTNULL);
			nHand += m_hArmHand[h] != INVALID_MODEL_NODE;
		}
		m_hArmRef = INVALID_MODEL_NODE;
		if(g_pLTClient->GetNextModelNode(hWeapon, INVALID_MODEL_NODE, &m_hArmRef) != LT_OK || m_hArmRef == m_hArmRoot)
			m_hArmRef = INVALID_MODEL_NODE;
		if(m_hArmRoot == INVALID_MODEL_NODE || m_hArmHand[0] == INVALID_MODEL_NODE || m_hArmRef == INVALID_MODEL_NODE)
			m_hArmRoot = INVALID_MODEL_NODE;
		g_pLTClient->ModelNodeControl(hWeapon, (m_hArmRoot != INVALID_MODEL_NODE && m_nArmSplit < 0) ? LeftArmNodeControl : LTNULL, this);
		m_bArmCopyPieces = LTFALSE;
		// With node control the engine works the model's extent out from its nodes, and the moved arm
		// then shifts where the model is lit from: moving the left hand changed the light on the whole
		// weapon (the smartgun, which has no right arm, most). This keeps its own extent.
		if(m_hArmRoot != INVALID_MODEL_NODE)
			g_pLTClient->SetObjectClientFlags(hWeapon, g_pLTClient->GetObjectClientFlags(hWeapon) | CF_INSIDERADIUS | CF_DONTSETDIMS);

		// avp2xr.ini [LeftHand] <weapon> = pitch yaw roll [forward up right]: this weapon's own
		// angle (and offset) for the hand, in place of the LeftHand* ones. Read now, so editing
		// the ini (and running install.bat) takes effect at the next change of weapon.
		m_vArmOffset = m_vLeftOffset;
		m_rArmAngle = m_rLeftAngle;
		char szValue[128] = "";
		const char *szName = pWeapon ? pWeapon->szName : "";
		if(m_szIni[0] && szName[0])
			GetPrivateProfileStringA("LeftHand", szName, "", szValue, sizeof(szValue), m_szIni);
		float f[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		int nValues = szValue[0] ? sscanf(szValue, "%f %f %f %f %f %f", &f[0], &f[1], &f[2], &f[3], &f[4], &f[5]) : 0;
		if(nValues >= 3)
			m_rArmAngle = GunAngleRotation(f[0], f[1], f[2]);
		if(nValues >= 6)
			m_vArmOffset = LTVector(f[5], f[4], f[3]) * (g_vtVRWorldScale.GetFloat() / 100.0f);
		VRLog("Left arm for %s: root %s, %d of 4 hand nodes, [LeftHand] %s=%s, %s", szName[0] ? szName : "?", szRoot, nHand,
			szName[0] ? szName : "?", nValues >= 3 ? szValue : "(not set: the LeftHand* settings)",
			m_nArmSplit >= 0 ? "drawn as a copy of the model (its own pieces)" : "moved by node control");
	}

	// Without the left controller (or a left arm) the arm's move stops; otherwise the last one
	// stays until this one is worked out
	if(!m_bLeftArm || !m_bOffWorld || m_hArmRoot == INVALID_MODEL_NODE)
	{
		m_bArmActive = LTFALSE;
		return;
	}

	// The reference node in model space, to find the node control's space from
	ModelLT *pModelLT = g_pLTClient->GetModelLT();
	LTransform tRef;
	if(pModelLT->GetNodeTransform(hWeapon, m_hArmRef, tRef, LTFALSE) != LT_OK)
		return;
	LTVector vRefPos;
	LTRotation rRef;
	g_pLTClient->GetTransformLT()->GetPos(tRef, vRefPos);
	g_pLTClient->GetTransformLT()->GetRot(tRef, rRef);
	LTMatrix mRef;
	g_pLTClient->GetMathLT()->SetupRotationMatrix(mRef, rRef);
	mRef.SetTranslation(vRefPos);
	m_mArmRefInv = mRef.MakeInverseTransform();

	// The animated hand: as the node control saw it, or before it has, from the model
	LTVector vHand[4];
	LTBOOL bHand[4];
	for(int h = 0; h < 4; h++)
	{
		bHand[h] = m_hArmHand[h] != INVALID_MODEL_NODE;
		if(!bHand[h])
			continue;
		if(m_bArmSeen && m_nArmSplit < 0)
			vHand[h] = m_vArmSeen[h];
		else
		{
			LTransform tTransform;
			bHand[h] = pModelLT->GetNodeTransform(hWeapon, m_hArmHand[h], tTransform, LTFALSE) == LT_OK;
			if(bHand[h])
				g_pLTClient->GetTransformLT()->GetPos(tTransform, vHand[h]);
		}
	}
	if(!bHand[0])
		return;
	if(!bHand[1])
		vHand[1] = vHand[0];

	// Held from the idle pose, like the right hand's palm: the palm, and the frames the hand is
	// turned by. The left hand is turned to mirror the right one (how the right hand holds the
	// weapon is how the left one holds its controller), since each weapon's animation has the
	// left hand at its own angle.
	if(nState == W_IDLE || !m_bArmHeld)
	{
		m_vArmPalm = (vHand[0] + vHand[1]) * 0.5f;
		m_bArmHeld = nState == W_IDLE;

		HandVectors left, right;
		LTVector vRight[4];
		m_bArmMatch = m_bLeftMatch && bHand[1] && bHand[2] && bHand[3] &&
			GetHandVectors(vHand[0], vHand[1], vHand[2], vHand[3], left) &&
			FindModelNode(hWeapon, s_szWristNodes, 4, vRight[0]) && FindModelNode(hWeapon, s_szKnuckleNodes, 4, vRight[1]) &&
			FindModelNode(hWeapon, s_szPointerNodes, 4, vRight[2]) && FindModelNode(hWeapon, s_szPinkyNodes, 4, vRight[3]) &&
			GetHandVectors(vRight[0], vRight[1], vRight[2], vRight[3], right);
		if(m_bArmMatch)
		{
			m_rArmHand = QuatFromBasis(left.vR, left.vU, left.vF);
			// The right hand in a mirror across the model's X: right and forward mirrored, up
			// mirrored and reversed (keeps it a rotation; it's what a left hand held the same way
			// gives from its knuckles)
			LTVector vR(-right.vR.x, right.vR.y, right.vR.z);
			LTVector vU(right.vU.x, -right.vU.y, -right.vU.z);
			LTVector vF(-right.vF.x, right.vF.y, right.vF.z);
			m_rArmMirror = QuatFromBasis(vR, vU, vF);
		}
	}

	// L = the left controller's frame for the hand; the arm turns by R^-1 L, and with the hand
	// matched also from the left hand's own frame to the mirrored right one
	LTRotation rToModel = m_rAimRot.Conjugate();
	LTRotation rLeft = m_rOffRot * m_rArmAngle;
	LTRotation rArm = rToModel * rLeft;
	if(m_bArmMatch)
		rArm = rArm * m_rArmMirror * m_rArmHand.Conjugate();
	LTVector vTarget = Rotate(rToModel, m_vOffPos + Rotate(rLeft, m_vArmOffset) - m_vWeaponOrigin) / m_fWeaponScale;

	// Two-handed, the gun points at the left hand, so the barrel runs through its palm: the hand
	// goes down (the gun's down, which is model space's) under it, eased over a moment.
	LTFLOAT fStep = g_pLTClient->GetFrameTime() / 0.15f;
	if(m_bTwoHanded)
		m_fTwoHandBlend = (m_fTwoHandBlend + fStep < 1.0f) ? m_fTwoHandBlend + fStep : 1.0f;
	else
		m_fTwoHandBlend = (m_fTwoHandBlend - fStep > 0.0f) ? m_fTwoHandBlend - fStep : 0.0f;
	vTarget.y -= m_fTwoHandBlend * m_fTwoHandDown / m_fWeaponScale;
	LTVector vR = Rotate(rArm, LTVector(1.0f, 0.0f, 0.0f));
	LTVector vU = Rotate(rArm, LTVector(0.0f, 1.0f, 0.0f));
	LTVector vF = Rotate(rArm, LTVector(0.0f, 0.0f, 1.0f));
	m_mArm.SetBasisVectors(&vR, &vU, &vF);
	m_mArm.SetTranslation(vTarget - Rotate(rArm, m_vArmPalm));
	m_mArmInv = m_mArm.MakeInverseTransform();

	// Split models: the copy goes where the arm's move would put the whole model (a model-space
	// point p at O + R S (rArm p + t), so the copy's origin is O + R S t and its rotation R rArm),
	// posed like the weapon model, showing only the arm
	if(m_nArmSplit >= 0 && m_hArmCopy && m_bWeaponView)
	{
		PlaceArmCopy(hWeapon, vTarget - Rotate(rArm, m_vArmPalm), rArm);
		m_bArmActive = LTFALSE;
		return;
	}

	m_bArmActive = LTTRUE;
	m_nArmTime = GetTickCount();

	// The engine only poses the model again when its animation moves, and the idle weapon holds
	// still, so it would keep drawing an old pose (the arm stuck where it was, moving with the
	// right hand). A 1 ms nudge of the animation time (back and forth, so it doesn't drift) makes
	// the pose stale, and asking for a node poses it now, with this frame's move; the eyes then
	// draw that pose.
	// The time is put back straight after, so no animation key is skipped: keys fire the weapons
	// (the fire key at the start of the pistol's fire animation) and turn their effects on and off
	// (the flamethrower's pilot light comes back on at the start of its idle). And only while idle;
	// otherwise the animation is moving anyway, and the engine poses it again during the game's
	// update (with the last frame's move).
	if(nState != W_IDLE || !m_bArmForcePose)
	{
		m_bArmDrawing = LTTRUE;
		return;
	}
	LTAnimTracker *pTracker = LTNULL;
	int nPassesBefore = m_nArmPasses[0];
	uint32 nRestoreTime = 0;
	LTBOOL bRestoreTime = LTFALSE;
	if(pModelLT->GetMainTracker(hWeapon, pTracker) == LT_OK && pTracker)
	{
		uint32 nTime = 0, nLength = 0;
		pModelLT->GetCurAnimTime(pTracker, nTime);
		pModelLT->GetCurAnimLength(pTracker, nLength);
		m_nArmNudge = -m_nArmNudge;
		uint32 nNudged = nTime;
		if(m_nArmNudge > 0 && nTime + 1 <= nLength)
			nNudged = nTime + 1;
		else if(nTime > 0)
			nNudged = nTime - 1;
		else if(nLength > 0)
			nNudged = 1;
		pModelLT->SetCurAnimTime(pTracker, nNudged);
		nRestoreTime = nTime;
		bRestoreTime = nNudged != nTime;
	}
	LTransform tPosed;
	pModelLT->GetNodeTransform(hWeapon, m_hArmHand[0], tPosed, LTFALSE);
	if(bRestoreTime)
		pModelLT->SetCurAnimTime(pTracker, nRestoreTime);
	LTBOOL bForced = m_nArmPasses[0] > nPassesBefore;
	if(m_nArmForced != (int)bForced)
	{
		m_nArmForced = bForced;
		VRLog("Left arm: forcing a new pose each frame %s; hand %s", bForced ? "works" : "DOESN'T WORK (the arm will lag or stick)",
			m_bArmMatch ? "turned to mirror the right one" : "at its animated angle");
	}
	m_bArmDrawing = LTTRUE;

	static uint32 s_nNextLog = 0;
	uint32 nNow = GetTickCount();
	if((int32)(nNow - s_nNextLog) >= 0)
	{
		s_nNextLog = nNow + 2000;
		LTVector vDims(0.0f, 0.0f, 0.0f);
		g_pLTClient->Physics()->GetObjectDims(hWeapon, &vDims);
		VRLog("Left arm: palm (%.2f %.2f %.2f)%s -> (%.2f %.2f %.2f) in model space, matched %d, passes %d in the game, %d while drawing, "
			"weapon dims (%.2f %.2f %.2f) flags %x",
			m_vArmPalm.x, m_vArmPalm.y, m_vArmPalm.z, m_bArmHeld ? " idle" : "", vTarget.x, vTarget.y, vTarget.z, m_bArmMatch,
			m_nArmPasses[0], m_nArmPasses[1], vDims.x, vDims.y, vDims.z, g_pLTClient->GetObjectClientFlags(hWeapon));
		m_nArmPasses[0] = m_nArmPasses[1] = 0;
	}
}

// Called by the engine for each node of the model as it works out the pose (parents first), with
// the node's transform in the space the model is being posed in: its placement is included (the
// view-model placement for each eye, or the game's own when it asks for sockets). That space is
// found from the reference node (the model's root, first in the pass), whose model-space
// transform is known.
void VRMgr::LeftArmNodeControl(HOBJECT hObj, HMODELNODE hNode, LTMatrix *pGlobalMat, void *pUserData)
{
	VRMgr *pMgr = (VRMgr*)pUserData;
	if(hObj != pMgr->m_hArmModel || hNode == INVALID_MODEL_NODE)
		return;
	if(hNode == pMgr->m_hArmRef)
	{
		pMgr->m_nArmPasses[pMgr->m_bArmDrawing ? 1 : 0]++;
		pMgr->m_mArmPass = *pGlobalMat * pMgr->m_mArmRefInv;
		pMgr->m_mArmPassInv = pMgr->m_mArmPass;
		pMgr->m_bArmPass = pMgr->m_mArmPassInv.Inverse();
		pMgr->m_bArmApplied = LTFALSE;
		if(pMgr->m_bArmPass && pMgr->m_bArmActive)
		{
			pMgr->m_mArmPassMove = pMgr->m_mArmPass * pMgr->m_mArm * pMgr->m_mArmPassInv;
			pMgr->m_bArmApplied = LTTRUE;
		}
		return;
	}
	if(hNode == pMgr->m_hArmRoot)
	{
		if(pMgr->m_bArmApplied)
			*pGlobalMat = pMgr->m_mArmPassMove * (*pGlobalMat);
		return;
	}
	if(!pMgr->m_bArmPass)
		return;
	for(int h = 0; h < 4; h++)
	{
		if(hNode != pMgr->m_hArmHand[h])
			continue;
		// Where the animation has it in model space: the pass's space and the arm's move taken out
		LTVector vPos;
		pGlobalMat->GetTranslation(vPos);
		pMgr->m_mArmPassInv.Apply(vPos);
		if(pMgr->m_bArmApplied)
			pMgr->m_mArmInv.Apply(vPos);
		pMgr->m_vArmSeen[h] = vPos;
		if(h == 0)
			pMgr->m_bArmSeen = LTTRUE;	// the fingers follow in the same pass (they're below the wrist)
		break;
	}
}

// ----------------------------------------------------------------------- //
// The railgun zoom's green overlay normally covers the view (a really-close sprite straight ahead
// of the camera). In VR the gun aims on its own, so the overlay follows it:
//   head: it stays on the view, but each eye's copy is moved (at its own depth) so its centre is on
//         the line from that eye to where the shots hit; the centre then also looks as far away as
//         the target. It's drawn larger (RailScopeScale) so its edge stays out of view as it moves.
//   gun:  a panel ahead of the muzzle along the aim, facing the eye, placed in each eye's view the
//         way the weapon is (at 1/S of the way, so it has the right depth), like a scope.

void VRMgr::PlaceRailOverlay(const LTVector &vEyePos, const LTRotation &rEyeRot)
{
	if(!m_nRailScope || !m_bMuzzleWorld)
		return;
	LTFLOAT fSize = 0.0f;
	HOBJECT hRail = g_pGameClientShell->GetVisionModeMgr()->GetRailOverlay(fSize);
	if(!hRail || fSize <= 0.0f)
		return;
	if(hRail != m_hRailOverlay)
	{
		m_hRailOverlay = hRail;
		g_pLTClient->GetObjectPos(hRail, &m_vRailPos);
		g_pLTClient->GetObjectScale(hRail, &m_vRailScale);
	}

	if(m_nRailScope == 1)
	{
		LTVector vTarget = m_bCrosshairHit ? m_vCrosshairPos :
			m_vShotPos + Rotate(m_rAimRot, LTVector(0.0f, 0.0f, VR_CROSSHAIR_RANGE));
		LTVector vDir = Rotate(rEyeRot.Conjugate(), vTarget - vEyePos);
		if(vDir.z < 1.0f)
			return;	// behind the eye: leave it where the game has it
		LTFLOAT fDepth = m_vRailPos.z > 0.0f ? m_vRailPos.z : 33.0f;
		LTVector vHeadPos(vDir.x * fDepth / vDir.z, vDir.y * fDepth / vDir.z, fDepth);
		LTVector vHeadScale(m_vRailScale.x * m_fRailScale, m_vRailScale.y * m_fRailScale, m_vRailScale.z);
		g_pLTClient->SetObjectPos(hRail, &vHeadPos, LTTRUE);
		g_pLTClient->SetObjectScale(hRail, &vHeadScale);
		return;
	}
	if(m_fRailSize <= 0.0f)
		return;

	LTVector vCentre = m_vShotPos + Rotate(m_rAimRot, LTVector(0.0f, 0.0f, m_fRailDistance));
	LTVector vPos = Rotate(rEyeRot.Conjugate(), vCentre - vEyePos) / m_fWeaponScale;
	LTFLOAT fHalf = m_fRailSize * 0.5f / m_fWeaponScale / fSize;
	LTVector vScale(fHalf, fHalf, 1.0f);
	g_pLTClient->SetObjectPos(hRail, &vPos, LTTRUE);
	g_pLTClient->SetObjectScale(hRail, &vScale);
}

// ----------------------------------------------------------------------- //
// The left-arm copy of the weapon model (for models whose left arm is in pieces of its own): the
// same files as the weapon model, only the arm's pieces shown, at the left controller. Lit from its
// own NormalRef, which moves with it, so each arm is lit from its own side (with node control the
// model has one lighting reference, and the arm that isn't on it was lit as the other hand moved).

void VRMgr::OnWeaponModelFiles(HOBJECT hWeapon, const ObjectCreateStruct *pStruct)
{
	if(!hWeapon || !pStruct)
		return;
	const char *szFile = pStruct->m_Filename;
	const char *szBase = strrchr(szFile, '\\');
	const char *szBase2 = strrchr(szFile, '/');
	if(szBase2 > szBase)
		szBase = szBase2;
	szBase = szBase ? szBase + 1 : szFile;
	m_nArmSplit = -1;
	for(int i = 0; i < (int)(sizeof(s_ArmPieces) / sizeof(s_ArmPieces[0])); i++)
		if(!stricmp(s_ArmPieces[i].szModel, szBase))
			m_nArmSplit = i;
	m_bArmCopyPieces = LTFALSE;
	m_bArmCopyActive = LTFALSE;
	m_nArmMainPieces = 0;

	if(m_nArmSplit < 0)
	{
		OnWeaponModelRemoved();
		return;
	}
	if(!m_hArmCopy)
	{
		ObjectCreateStruct cs = *pStruct;
		cs.m_ObjectType = OT_MODEL;
		cs.m_Flags = FLAG_REALLYCLOSE;
		cs.m_Flags2 = FLAG2_PORTALINVISIBLE | FLAG2_DYNAMICDIRLIGHT;
		m_hArmCopy = g_pLTClient->CreateObject(&cs);
		if(m_hArmCopy)
			g_pLTClient->SetObjectColor(m_hArmCopy, 1.0f, 1.0f, 1.0f, 1.0f);
	}
	else
		g_pLTClient->Common()->SetObjectFilenames(m_hArmCopy, (ObjectCreateStruct*)pStruct);
	if(m_hArmCopy)
		g_pLTClient->SetModelLooping(m_hArmCopy, LTFALSE);
	VRLog("Left arm copy for %s: %s", szBase, m_hArmCopy ? "made" : "COULDN'T BE MADE");
}

void VRMgr::OnWeaponModelRemoved()
{
	if(m_hArmCopy)
		g_pLTClient->DeleteObject(m_hArmCopy);
	m_hArmCopy = LTNULL;
	m_bArmCopyPieces = LTFALSE;
	m_bArmCopyActive = LTFALSE;
	m_nArmMainPieces = 0;
}

void VRMgr::PlaceArmCopy(HOBJECT hWeapon, const LTVector &vMove, const LTRotation &rArm)
{
	ModelLT *pModelLT = g_pLTClient->GetModelLT();

	// Its pieces: only the arm shown on the copy; the arm's pieces on the weapon model, to hide
	if(!m_bArmCopyPieces)
	{
		char aNames[32][48];
		int nLeft = SplitNames(s_ArmPieces[m_nArmSplit].szLeft, aNames, 32);
		m_nArmMainPieces = 0;
		for(int i = 0; i < nLeft; i++)
		{
			HMODELPIECE hPiece;
			if(pModelLT->GetPiece(m_hArmCopy, aNames[i], hPiece) == LT_OK)
				pModelLT->SetPieceHideStatus(m_hArmCopy, hPiece, LTFALSE);
			if(m_nArmMainPieces < 8 && pModelLT->GetPiece(hWeapon, aNames[i], hPiece) == LT_OK)
				m_hArmMainPieces[m_nArmMainPieces++] = hPiece;
		}
		int nOther = SplitNames(s_ArmPieces[m_nArmSplit].szOther, aNames, 32);
		int nHidden = 0;
		for(int i = 0; i < nOther; i++)
		{
			HMODELPIECE hPiece;
			if(pModelLT->GetPiece(m_hArmCopy, aNames[i], hPiece) == LT_OK &&
			   pModelLT->SetPieceHideStatus(m_hArmCopy, hPiece, LTTRUE) != LT_ERROR)
				nHidden++;
		}
		m_bArmCopyPieces = LTTRUE;
		VRLog("Left arm copy: %d of %d arm pieces found on the weapon model, %d of %d other pieces hidden on the copy",
			m_nArmMainPieces, nLeft, nHidden, nOther);

		// The arm's pieces aren't there (the split model copies in vrrez not installed): move the
		// arm's nodes instead, as for models with shared pieces
		if(!m_nArmMainPieces)
		{
			VRLog("Left arm copy: the model has no separate arm pieces; moving the arm's nodes instead");
			m_nArmSplit = -1;
			OnWeaponModelRemoved();
			g_pLTClient->ModelNodeControl(hWeapon, LeftArmNodeControl, this);
			return;
		}
	}

	// Posed like the weapon model: same animation and time (the copy doesn't send model keys)
	LTAnimTracker *pMain = LTNULL, *pCopy = LTNULL;
	if(pModelLT->GetMainTracker(hWeapon, pMain) == LT_OK && pMain &&
	   pModelLT->GetMainTracker(m_hArmCopy, pCopy) == LT_OK && pCopy)
	{
		HMODELANIM hAnim = INVALID_MODEL_ANIM, hCopyAnim = INVALID_MODEL_ANIM;
		pModelLT->GetCurAnim(pMain, hAnim);
		pModelLT->GetCurAnim(pCopy, hCopyAnim);
		if(hAnim != hCopyAnim && hAnim != INVALID_MODEL_ANIM)
			pModelLT->SetCurAnim(pCopy, hAnim);
		uint32 nTime = 0;
		pModelLT->GetCurAnimTime(pMain, nTime);
		pModelLT->SetCurAnimTime(pCopy, nTime);
	}

	// Seen the same way (flags: visibility, translucency, environment map; colour: cloaking)
	g_pLTClient->SetObjectFlags(m_hArmCopy, m_nWeaponFlags);
	float r, g, b, a;
	g_pLTClient->GetObjectColor(hWeapon, &r, &g, &b, &a);
	g_pLTClient->SetObjectColor(m_hArmCopy, r, g, b, a);

	for(int i = 0; i < m_nArmMainPieces; i++)
		pModelLT->SetPieceHideStatus(hWeapon, m_hArmMainPieces[i], LTTRUE);

	m_vArmCopyOrigin = m_vWeaponOrigin + Rotate(m_rAimRot, vMove * m_fWeaponScale);
	m_rArmCopyRot = m_rAimRot * rArm;
	m_bArmCopyActive = LTTRUE;
}

// ----------------------------------------------------------------------- //

void VRMgr::RestoreWeapon()
{
	if(!m_bWeaponMoved)
		return;

	CWeaponModel *pWeapon = g_pGameClientShell->GetWeaponModel();
	HOBJECT hWeapon = pWeapon ? pWeapon->GetHandle() : LTNULL;
	if(hWeapon)
	{
		g_pLTClient->SetObjectFlags(hWeapon, m_nWeaponFlags);
		g_pLTClient->SetObjectScale(hWeapon, &m_vWeaponScale);
		g_pLTClient->SetObjectPos(hWeapon, &m_vWeaponPos, LTTRUE);
		g_pLTClient->SetObjectRotation(hWeapon, &m_rWeaponRot);
	}
	CMuzzleFlashFX *pFlash = pWeapon ? pWeapon->GetMuzzleFlash() : LTNULL;
	if(pFlash)
		pFlash->EndVRScale();
	m_bArmDrawing = LTFALSE;
	if(m_hArmCopy)
		g_pLTClient->SetObjectFlags(m_hArmCopy, g_pLTClient->GetObjectFlags(m_hArmCopy) & ~FLAG_VISIBLE);
	if(m_bArmCopyActive && hWeapon)
	{
		for(int i = 0; i < m_nArmMainPieces; i++)
			g_pLTClient->GetModelLT()->SetPieceHideStatus(hWeapon, m_hArmMainPieces[i], LTFALSE);
	}
	m_bArmCopyActive = LTFALSE;

	// The rail overlay back where the game keeps it (if it's still there)
	if(m_hRailOverlay)
	{
		LTFLOAT fSize = 0.0f;
		if(g_pGameClientShell->GetVisionModeMgr()->GetRailOverlay(fSize) == m_hRailOverlay)
		{
			g_pLTClient->SetObjectPos(m_hRailOverlay, &m_vRailPos, LTTRUE);
			g_pLTClient->SetObjectScale(m_hRailOverlay, &m_vRailScale);
		}
		m_hRailOverlay = LTNULL;
	}
	if(m_bWeaponView)
	{
		SetConsoleFloat("ExtraFOVXOffset", m_fExtraFOVX);
		SetConsoleFloat("ExtraFOVYOffset", m_fExtraFOVY);
	}
	m_bWeaponMoved = LTFALSE;
}

// ----------------------------------------------------------------------- //

void VRMgr::ProfileMark(ProfileStage eStage)
{
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	m_Prof[eStage] = t;
	if(eStage != PROF_POSTUPDATE)
		return;

	// A frame runs PreUpdate .. PostUpdate (the client), then the engine flips the HUD image and
	// updates the world (and the local server) before the next PreUpdate. Only runs of whole
	// stereo frames are counted.
	if(!m_bLastFrameStereo || m_Prof[PROF_FLIPPED].QuadPart <= m_Prof[PROF_BEGIN].QuadPart ||
	   m_Prof[PROF_BEGIN].QuadPart < m_Prof[PROF_PREUPDATE].QuadPart)
	{
		m_ProfLastPre.QuadPart = 0;
		return;
	}

	LARGE_INTEGER f;
	QueryPerformanceFrequency(&f);
	double fMs = 1000.0 / (double)f.QuadPart;
	if(m_ProfLastPre.QuadPart)
	{
		double d[9];
		d[0] = (m_Prof[PROF_BEGIN].QuadPart - m_Prof[PROF_PREUPDATE].QuadPart) * fMs;		// client update before rendering
		d[1] = (m_Prof[PROF_WAITED].QuadPart - m_Prof[PROF_BEGIN].QuadPart) * fMs;		// xrWaitFrame
		d[2] = (m_Prof[PROF_RENDERED].QuadPart - m_Prof[PROF_WAITED].QuadPart) * fMs;		// both eyes' RenderCamera
		d[3] = (m_Prof[PROF_END3D].QuadPart - m_Prof[PROF_RENDERED].QuadPart) * fMs;		// End3D (EndScene)
		d[4] = (m_Prof[PROF_FLIPPED].QuadPart - m_Prof[PROF_END3D].QuadPart) * fMs;		// FlipScreen of the eyes
		d[5] = (m_Prof[PROF_END].QuadPart - m_Prof[PROF_FLIPPED].QuadPart) * fMs;			// HUD drawing
		d[6] = (m_Prof[PROF_POSTUPDATE].QuadPart - m_Prof[PROF_END].QuadPart) * fMs;		// rest of the client update
		d[7] = (m_Prof[PROF_PREUPDATE].QuadPart - m_ProfLastPost.QuadPart) * fMs;			// engine: HUD flip, world, server
		d[8] = (m_Prof[PROF_PREUPDATE].QuadPart - m_ProfLastPre.QuadPart) * fMs;			// whole frame
		for(int i = 0; i < 9; i++)
			m_fProfSum[i] += d[i];
		if(d[8] > m_fProfMaxFrame)
			m_fProfMaxFrame = d[8];
		if(!m_nProfFrames++)
			m_nProfStart = GetTickCount();
	}
	m_ProfLastPre = m_Prof[PROF_PREUPDATE];
	m_ProfLastPost = m_Prof[PROF_POSTUPDATE];

	if(m_nProfFrames && GetTickCount() - m_nProfStart >= 2000)
	{
		double n = m_nProfFrames;
		VRLog("Frame timing (ms, %d frames): frame %.1f (max %.1f) = client pre %.1f + wait %.1f + eyes %.1f + End3D %.1f + "
			"eye flip %.1f + HUD %.1f + client post %.1f + engine (HUD flip, world, server) %.1f",
			m_nProfFrames, m_fProfSum[8] / n, m_fProfMaxFrame, m_fProfSum[0] / n, m_fProfSum[1] / n, m_fProfSum[2] / n,
			m_fProfSum[3] / n, m_fProfSum[4] / n, m_fProfSum[5] / n, m_fProfSum[6] / n, m_fProfSum[7] / n);
		memset(m_fProfSum, 0, sizeof(m_fProfSum));
		m_fProfMaxFrame = 0.0;
		m_nProfFrames = 0;
	}
}

// ----------------------------------------------------------------------- //

void VRLog(const char *szFormat, ...)
{
	static FILE *s_pLog = LTNULL;
	static LTBOOL s_bTried = LTFALSE;
	if(!s_pLog && !s_bTried)
	{
		s_bTried = LTTRUE;
		char szPath[MAX_PATH];
		if(GetEnvironmentVariableA("LOCALAPPDATA", szPath, MAX_PATH))
		{
			strcat(szPath, "\\avp2xr");
			CreateDirectoryA(szPath, LTNULL);
			strcat(szPath, "\\cshell.log");
			s_pLog = fopen(szPath, "w");
		}
	}
	if(!s_pLog)
		return;

	SYSTEMTIME st;
	GetLocalTime(&st);
	fprintf(s_pLog, "%02d:%02d:%02d.%03d ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	va_list args;
	va_start(args, szFormat);
	vfprintf(s_pLog, szFormat, args);
	va_end(args);
	fputc('\n', s_pLog);
	fflush(s_pLog);
}
