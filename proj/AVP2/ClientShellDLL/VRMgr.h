// ----------------------------------------------------------------------- //
//
// MODULE  : VRMgr.h
//
// PURPOSE : Stereo rendering for VR through the avp2xr d3d11.dll proxy
//
// ----------------------------------------------------------------------- //

#ifndef __VR_MGR_H__
#define __VR_MGR_H__

#include "ltbasedefs.h"
#include "cameramgrdefs.h"
#include "../../avp2xr/avp2xr_api.h"

// ----------------------------------------------------------------------- //
// Each eye is rendered into its own half of the back buffer (left eye on the left). The
// avp2xr proxy hands the halves to OpenXR as a projection layer.
//
// Console variables:
//   VREnable      0 = always render normally (the proxy then shows the flat virtual screen)
//   VRWorldScale  world units per metre, used for head movement and eye separation
//   VRSnapTurn    degrees per right-stick snap turn (default 45, 0 = off)
//   VRGunScale    size of the first-person weapon and hands (default 12; the models are built tiny)
//   VRGunMode     1 = view model placed per eye (default), 0 = scaled-up world object
//   VRGripX/Y/Z   palm position in unrigged first-person models (model units, before VRGunScale)
//   VRGunOffsetX/Y/Z  moves the weapon in the controller's frame (world units: right, up, forward;
//                 added to the palm's place at the controller's grip pose)
//   VRTwoHanded   1 = with the left hand in front of the right, the gun aims from right hand to left
//
// avp2xr.ini [VR] LeftArm (yes) puts the first-person model's left arm on the left controller;
// LeftHandForward/Up/Right (cm) and LeftHandPitch/Yaw/Roll (degrees) fit the hand to it, and
// default to the Gun* settings mirrored.
//
// The player's first-person view turns only with the head; the mouse (later the controllers)
// only aims the body. Other cameras (cutscenes) keep their own rotation with the head on top.

class VRMgr
{
	public:
		VRMgr();

		void		Init();

		// Call once per frame in every game state (menus too).
		void		Update();

		// Call once per rendered frame before the cameras render. Returns LTTRUE if this
		// frame should be rendered in stereo.
		LTBOOL		BeginFrame();
		LTBOOL		IsStereo() const		{ return m_bStereo; }

		// Shrink each eye's view by these fractions (0-1) across and down, about its centre, with its
		// FOV narrowed to match (so the world stays in place): camera effects that shrink the view
		// (the vision mode change's wipe) are shown that way in the headset, with black around it
		void		SetEyeInset(LTFLOAT fX, LTFLOAT fY)	{ m_fEyeInsetX = fX; m_fEyeInsetY = fY; }

		// A zoom, as the ratio of the zoomed FOV's tangent to the normal one (1 = none): each eye is
		// rendered with its FOV narrowed by it but shown over its whole FOV, so the view is magnified
		// (avp2xr.ini ZoomStrength scales it). The first-person weapon keeps its size.
		void		SetEyeZoom(LTFLOAT fZoom)				{ m_fEyeZoom = fZoom; }

		// avp2xr.ini ZoomSteps: how many zoom levels the Zoom button steps through (1-3)
		int			GetZoomSteps() const					{ return m_nZoomSteps; }

		// LTTRUE while VR is in use (the avp2xr proxy is there and VREnable is on): the mouse
		// cursor is then kept hidden, since the controllers work the menus.
		LTBOOL		HidesCursor() const;

		// LTTRUE while VR is in use and avp2xr.ini's WeaponIdleAnimations is no: the first-person
		// weapon then holds still while idle instead of swaying and fidgeting in the hand.
		LTBOOL		StopsWeaponIdle() const;

		// LTTRUE while VR is in use and avp2xr.ini's WeaponSwitchAnimations is no: weapons change
		// at once, without being lowered and raised
		LTBOOL		SkipsWeaponSwitchAnims() const;

		// LTTRUE while VR is in use and avp2xr.ini's AlwaysRun is yes: the player always runs
		LTBOOL		AlwaysRuns() const					{ return m_pApi && m_bAlwaysRun; }

		// The camera that shows the player's own view.
		void		SetPlayerCamera(HCAMERA hCamera)	{ m_hPlayerCamera = hCamera; }

		// Works out one eye's camera from the camera's mono pose. nEye is 0 (left) or 1 (right).
		// rRect receives the eye's screen rectangle.
		void		GetEyeCamera(int nEye, HCAMERA hCamera, const LTVector &vPos, const LTRotation &rRot,
								 LTVector &vEyePos, LTRotation &rEyeRot,
								 LTFLOAT &fFOVX, LTFLOAT &fFOVY, LTRect &rRect);

		// Call when a camera has been rendered for both eyes.
		void		MarkStereoRendered()	{ m_bRendered = LTTRUE; }
		LTBOOL		IsStereoRendered() const	{ return m_bStereo && m_bRendered; }

		// Call after the cameras have rendered and before flipping the eye image. With
		// bHudFollows the game must then clear the screen, draw only the HUD and flip again;
		// that image goes to the headset's HUD panel instead of the monitor.
		void		SubmitEyes(LTBOOL bHudFollows);

		// Call at the end of the frame's drawing.
		void		EndFrame();

		// Frame timing for the log: marks in the order the frame reaches them. Stereo frames
		// are averaged and written to cshell.log every couple of seconds.
		enum ProfileStage { PROF_PREUPDATE, PROF_BEGIN, PROF_WAITED, PROF_RENDERED, PROF_END3D, PROF_FLIPPED,
							PROF_END, PROF_POSTUPDATE, PROF_COUNT };
		void		ProfileMark(ProfileStage eStage);

		// While the last frame was rendered in stereo, returns LTTRUE with tan(half FOV) of the
		// eye views, for sizing overlays that must cover the whole view (vision modes).
		LTBOOL		GetStereoFOVTangents(LTFLOAT &fTanX, LTFLOAT &fTanY) const;

		// Controller input while the player's view is in stereo. Both return LTFALSE when the
		// input isn't available, so the keyboard and mouse work as normal.

		// World-space horizontal direction to move in, from the left stick relative to where
		// the head faces. Its length (0 to 1) is how far the stick is pushed.
		LTBOOL		GetMoveDirection(LTVector &vDir) const;

		// LTTRUE while adjust mode has the controllers (the sticks and buttons aren't the game's)
		LTBOOL		IsAdjusting() const					{ return m_bAdjusting; }
		// The left stick as it's pushed (past the dead zone, 0-1 each way): right and forward
		LTBOOL		GetMoveStick(LTFLOAT &fRight, LTFLOAT &fForward) const;

		// World-space unit vector the aiming (right) controller points along.
		LTBOOL		GetAimDirection(LTVector &vDir) const;

		// LTTRUE while an AVP2XR_BTN_* controller button is held (for control flags such as
		// fire, jump and crouch). Presses and releases are also sent to the game as
		// OnCommandOn/OnCommandOff from Update(), the same as keys.
		LTBOOL		IsButtonHeld(unsigned int nButton) const	{ return (m_nButtons & nButton) != 0; }

		// World-space unit vector the head looks along. The player's body (yaw and aiming
		// pitch) follows this, so everything tied to the body matches the head: the motion
		// tracker, the flashlight, what other players see.
		LTBOOL		GetHeadDirection(LTVector &vDir) const;

		// Where the head is (between the eyes, real head movement included) and how it's turned in
		// the world, from the last stereo frame of the player's view: Use acts on what you look at
		// from here (the game's camera position leaves out leaning towards a switch).
		LTBOOL		GetHeadPose(LTVector &vPos, LTRotation &rRot) const;

		// avp2xr.ini UseAngle, degrees: Use (and its crosshair) finds something to use this far
		// off where the head looks, when nothing is straight ahead. 0 = straight ahead only.
		LTFLOAT		GetUseAngle() const					{ return m_fUseAngle; }

		// Where shots start (the weapon's muzzle; the controller for melee) and the aim, from the
		// last stereo frame. Shots are fired from here along its forward, instead of from the camera.
		LTBOOL		GetAimPose(LTVector &vPos, LTRotation &rRot) const;

		// World position of the first-person weapon's muzzle and the way it points, from the
		// last stereo frame (for effects that come out of it, like the flamethrower's flame).
		LTBOOL		GetMuzzlePose(LTVector &vPos, LTRotation &rRot) const;

		// Called by CameraMgr around the eye renders of the player camera: PlaceWeapon works out
		// where the first-person weapon goes at the aiming controller, PlaceWeaponForEye puts it
		// there in each eye's view, and RestoreWeapon puts the game's own placement back.
		void		PlaceWeapon();

		// The weapon's crosshair in each eye at the point the aim hits (so it has the right
		// depth): ProjectCrosshair per eye as it's rendered, DrawCrosshairs after both.
		void		ProjectCrosshair(int nEye, HCAMERA hCamera, const LTVector &vEyePos, const LTRotation &rEyeRot,
									 LTFLOAT fFOVX, LTFLOAT fFOVY, const LTRect &rRect);
		void		DrawCrosshairs();

		// Screen markers on things in the world (the Predator's shoulder cannon target): drawn into
		// each eye's view after both eyes have rendered, so they stay on the thing as the head turns
		// (on the head-locked HUD panel they swung about). ProjectToEye gives a world point's place
		// in that eye's view of the player camera this frame (fX, fY on the screen, inside rRect) and
		// how much bigger an image there is than on the flat screen at the same angle.
		void		DrawEyeOverlays();
		LTBOOL		ProjectToEye(int nEye, const LTVector &vWorld, LTFLOAT &fX, LTFLOAT &fY,
								 LTFLOAT &fScaleX, LTFLOAT &fScaleY, LTRect &rRect) const;
		// avp2xr 11 on draws the markers at the headset's resolution (the game's own 2D drawing is
		// blocky there): during DrawEyeOverlays, AddMarker instead of drawing. fX, fY and the half
		// size are on the screen (game pixels), the angle clockwise in radians.
		LTBOOL		CanAddMarkers() const				{ return m_pApi && m_pApi->version >= 11; }
		void		AddMarker(const char *szImage, HSURFACE hImage, int nEye, LTFLOAT fX, LTFLOAT fY,
							  LTFLOAT fHalfW, LTFLOAT fHalfH, LTFLOAT fAngle, LTFLOAT fAlpha);

		void		PlaceWeaponForEye(const LTVector &vEyePos, const LTRotation &rEyeRot, LTFLOAT fFOVX, LTFLOAT fFOVY);
		void		RestoreWeapon();

		// CWeaponModel calls these when its model object gets its files and when it's deleted, so
		// the left-arm copy (see PlaceLeftArm) matches it
		void		OnWeaponModelFiles(HOBJECT hWeapon, const struct ObjectCreateStruct *pStruct);
		void		OnWeaponModelRemoved();

	private:
		LTBOOL		FindApi();
		void		PlaceLeftArm(HOBJECT hWeapon, struct WEAPON *pWeapon, int nState);
		void		PlaceRailOverlay(const LTVector &vEyePos, const LTRotation &rEyeRot);
		void		PlaceArmCopy(HOBJECT hWeapon, const LTVector &vMove, const LTRotation &rArm);
		static void	LeftArmNodeControl(HOBJECT hObj, HMODELNODE hNode, LTMatrix *pGlobalMat, void *pUserData);
		static void	WeaponPoseNodeControl(HOBJECT hObj, HMODELNODE hNode, LTMatrix *pGlobalMat, void *pUserData);
		static void	ArmCopyNodeControl(HOBJECT hObj, HMODELNODE hNode, LTMatrix *pGlobalMat, void *pUserData);
		void		UpdateButtons();
		void		UpdateMenus();
		void		LeaveMenus();
		void		PressKey(int nKey);
		void		UpdateCutsceneSkip();

		// Each weapon's fit in the hands ([Gun] and [LeftHand]): read at each change of weapon, from
		// %LOCALAPPDATA%\avp2xr\hands.ini (what adjust mode saves) before avp2xr.ini
		void		ReadGunFit(const char *szWeapon);
		void		ReadLeftFit(const char *szWeapon);
		void		ApplyGunFit();
		void		ApplyLeftFit();
		LTBOOL		ReadFitLine(const char *szSection, const char *szWeapon, char *szValue, int nSize, const char **pszFrom);

		// Adjust mode (the Adjust button): the sticks fit the current weapon in the right or left
		// hand while the game gets no controller input; Adjust again saves it to hands.ini
		void		UpdateAdjust(unsigned int nRaw, unsigned int nPressed);
		void		EndAdjust();
		void		ShowAdjust(LTBOOL bNow);

		const Avp2XrApi	*m_pApi;
		uint32			m_nNextLookupTime;
		Avp2XrView		m_Views[2];
		Avp2XrEyeSubmit	m_Submit[2];
		LTBOOL			m_bStereo;
		LTFLOAT			m_fEyeInsetX;			// SetEyeInset
		LTFLOAT			m_fEyeZoom;				// SetEyeZoom
		LTFLOAT			m_fZoomStrength;		// avp2xr.ini ZoomStrength: 1 = the game's zoom, 0 = none
		int				m_nZoomSteps;			// avp2xr.ini ZoomSteps
		LTBOOL			m_bAlwaysRun;			// avp2xr.ini AlwaysRun
		LTBOOL			m_bSkipCutscenes;		// avp2xr.ini SpeedThroughCutscenes
		LTBOOL			m_bInCutscene;			// the cinematic camera was active last frame
		LTFLOAT			m_fNextSkipTime;		// game time to (re)send the skip at
		LTFLOAT			m_fShownFOVX;			// the FOV the last eye is shown with (unzoomed), for the weapon
		LTFLOAT			m_fShownFOVY;
		LTFLOAT			m_fEyeInsetY;
		LTBOOL			m_bRendered;
		LTBOOL			m_bSubmitted;
		LTBOOL			m_bLastFrameStereo;
		HCAMERA			m_hPlayerCamera;
		LTBOOL			m_bAnchorValid;
		LTFLOAT			m_fAnchorYaw;			// world yaw the player's view is anchored to
		float			m_fHeadRef[3];			// OpenXR head position at anchoring = the game camera position
		Avp2XrInput		m_Input;				// controller state for this frame
		LTBOOL			m_bInput;
		int				m_nRecenterCount;		// last runtime recenter count seen
		LTBOOL			m_bSnapArmed;			// the turn stick has returned to centre since the last snap turn
		LTBOOL			m_bCycleArmed;			// ...and since the last weapon cycle
		uint32			m_nCycleSelectTime;		// GetTickCount() to select the chosen weapon at, 0 = none
		LTBOOL			m_bMenuInput;			// the controllers are driving the menus
		LTBOOL			m_bShowCrosshair;		// avp2xr.ini [VR] ShowCrosshair
		LTBOOL			m_bSmoothCrosshair;		// ...CrosshairSmoothing: drawn from smoothed copies of the images
		// The railgun zoom's overlay (avp2xr.ini RailScope): 0 = as in the game, 1 = on the view but
		// centred where the gun aims (RailScopeScale times its size), 2 = on the gun, a panel of
		// m_fRailSize (world units) m_fRailDistance ahead of the muzzle
		int				m_nRailScope;
		LTFLOAT			m_fRailScale;
		LTFLOAT			m_fRailDistance;
		LTFLOAT			m_fRailSize;
		HOBJECT			m_hRailOverlay;			// moved for the eyes, to put back afterwards
		LTVector		m_vRailPos;
		LTVector		m_vRailScale;
		LTBOOL			m_bWeaponIdleAnims;		// avp2xr.ini [VR] WeaponIdleAnimations
		LTBOOL			m_bWeaponSwitchAnims;	// avp2xr.ini [VR] WeaponSwitchAnimations
		LTBOOL			m_bSmoothTurn;			// avp2xr.ini [VR] TurnMode = smooth (else snap)
		LTFLOAT			m_fSmoothTurnSpeed;		// ...SmoothTurnSpeed, degrees per second at full push
		LTRotation		m_rGunAngle;			// avp2xr.ini GunPitch/GunYaw/GunRoll, in the controller's frame
		LTRotation		m_rGunAngleCur;			// ...for the weapon in hand ([Gun] <weapon>, else m_rGunAngle)
		LTVector		m_vGunOffsetCur;		// its offset (world units: right, up, forward), else VRGunOffset
		// The fits as numbers: forward up right (cm), pitch yaw roll (degrees). The defaults from [VR]
		// (Gun*, LeftHand*), and the weapon in hand's (m_vGunOffsetCur and the rest are made from these)
		LTFLOAT			m_fGunDefault[6];
		LTFLOAT			m_fLeftDefault[6];
		LTFLOAT			m_fGunFit[6];
		LTFLOAT			m_fLeftFit[6];
		LTFLOAT			m_fMuzzleFit[6];		// [Muzzle] <weapon> = forward up right [pitch yaw roll] (cm, degrees, the
												// gun's frame): moves where shots, the flash and the flame start from, and
												// turns the flash (its sprites and particles; the shots keep to the aim)
		LTRotation		m_rMuzzleRot;			// the flash's turn in the world (the model's, then the [Muzzle] angle)
		char			m_szHandsIni[MAX_PATH];	// %LOCALAPPDATA%\avp2xr\hands.ini
		LTBOOL			m_bAdjusting;			// adjust mode is on
		LTBOOL			m_bAdjustLive;			// avp2xr.ini AdjustHandsLive: the Adjust button turns it on
		int				m_nAdjustHand;			// 0 = right ([Gun]), 1 = left ([LeftHand]), 2 = the muzzle ([Muzzle])
		LTBOOL			m_bAdjustMove;			// the sticks move the hand (else turn it)
		LTFLOAT			m_fAdjustStart[3][6];	// the fits when adjust mode began, for Undo
		LTBOOL			m_bAdjustChanged[3];	// ...and whether they've changed since
		uint32			m_nAdjustShowTime;		// GetTickCount() the values were last shown
		struct WEAPON	*m_pAdjustWeapon;		// the weapon being fitted
		unsigned int	m_nAdjustRaw;			// AVP2XR_BTN_* held last frame, before adjust mode takes them
		LTBOOL			m_bCrosshairHit;		// m_vCrosshairPos is set for this frame
		LTVector		m_vCrosshairPos;		// where the aim hits, in the world
		LTBOOL			m_bCrosshairEye[2];		// the crosshair is in front of that eye
		int				m_nCrosshairX[2];		// its centre on the screen, per eye
		int				m_nCrosshairY[2];
		LTFLOAT			m_fCrosshairX[2];		// ...exactly (avp2xr draws it there, API version 10 on)
		LTFLOAT			m_fCrosshairY[2];
		uint32			m_nCrosshairImageId;	// the crosshair image avp2xr was last given, 0 = none
		LTFLOAT			m_fCrosshairScaleX[2];	// its size in that eye relative to the flat screen
		LTFLOAT			m_fCrosshairScaleY[2];
		LTBOOL			m_bEyeView[2];			// the player camera's view in that eye, this frame (for ProjectToEye)
		LTVector		m_vEyeViewPos[2];
		LTRotation		m_rEyeViewRot[2];
		LTFLOAT			m_fEyeViewFOVX[2];
		LTFLOAT			m_fEyeViewFOVY[2];
		LTRect			m_rEyeViewRect[2];
		Avp2XrMarker	m_Markers[AVP2XR_MAX_MARKERS];	// AddMarker's, for this frame
		int				m_nMarkers;
		uint32			m_nMarkerImageId[AVP2XR_MAX_MARKER_IMAGES];	// the image avp2xr was given in each slot, 0 = none
		int				m_nNextMarkerSlot;		// the slot a new image goes in when all are used
		unsigned int	m_nMenuButtons;			// AVP2XR_BTN_* held, as last seen in the menus
		int				m_nMenuKey;				// arrow key the stick is holding down in the menus, 0 = none
		uint32			m_nMenuRepeatTime;		// GetTickCount() to repeat m_nMenuKey at
		unsigned int	m_nIgnoreButtons;		// held on leaving the menus; ignored in play until released
		LTBOOL			m_bTwoHanded;			// the off hand is steadying the gun (aim runs hand to hand)
		unsigned int	m_nButtons;				// AVP2XR_BTN_* held, as sent to the game
		LTBOOL			m_bObjectivesOn;		// the Objectives button's toggle: the objectives are shown
		LTBOOL			m_bLastFrameInput;		// m_Input holds usable input from the last stereo frame
		LTFLOAT			m_fHeadYaw;				// world yaw the head faced in the last stereo frame
		LTBOOL			m_bAimWorld;			// m_vAimPos/m_rAimRot are set for this frame
		LTBOOL			m_bLastFrameAimWorld;	// ...and were set in the last stereo frame
		LTBOOL			m_bMuzzleWorld;			// m_vMuzzlePos is set for this frame
		LTBOOL			m_bLastFrameMuzzle;		// ...and was set in the last stereo frame
		LTBOOL			m_bPlayerView;			// the player's own camera is rendered in stereo this frame
		LTBOOL			m_bLastFramePlayerView;
		LTRotation		m_rHeadWorld;			// head rotation in the world in the last stereo frame
		LTVector		m_vHeadPos;				// between the player view's eyes in the world, this frame
		LTVector		m_vHeadWorld;			// ...in the last stereo frame
		LTFLOAT			m_fUseAngle;			// avp2xr.ini UseAngle
		LTVector		m_vAimPos;				// aiming controller pose in the world
		LTBOOL			m_bGripWorld;			// m_vGripPos is set for this frame
		LTVector		m_vGripPos;				// the aiming controller's grip pose (palm) in the world
		LTRotation		m_rAimRot;				// how the weapon model is turned: the aim pose and the [Gun] angle
		LTRotation		m_rShotRot;				// where it aims (shots, crosshair, flame): the aim pose alone, so
												// the [Gun] angle only turns the model; hand to hand when two-handed
		LTBOOL			m_bWeaponMoved;
		LTVector		m_vWeaponPos;			// the weapon's own (view-relative) placement, to restore
		LTRotation		m_rWeaponRot;
		LTVector		m_vWeaponScale;
		uint32			m_nWeaponFlags;
		LTBOOL			m_bWeaponView;			// drawn as a really-close view model, placed per eye
		LTVector		m_vWeaponOrigin;		// where the model's origin goes in the world at full size
		LTFLOAT			m_fWeaponScale;			// VRGunScale this frame
		LTVector		m_vMuzzlePos;			// the weapon's muzzle in the world at full size
		HMODELNODE		m_hMuzzleNode;			// a node used as the muzzle (s_MuzzleNodes), INVALID_MODEL_NODE = none
		HMODELSOCKET	m_hFlashSocket;			// a socket the muzzle flash goes on instead (s_MuzzleNodes)
		LTBOOL			m_bFlashSocket;			// ...found this frame, at:
		LTVector		m_vFlashSocket;			// its place in model space
		LTVector		m_vShotPos;				// where shots start: the muzzle, or the controller for melee
		LTFLOAT			m_fExtraFOVX;			// the game's view-model FOV offsets, to restore
		LTFLOAT			m_fExtraFOVY;
		struct WEAPON		*m_pGripWeapon;			// weapon m_vGripPalm belongs to
		LTBOOL			m_bGripHeld;			// m_vGripPalm is from the idle pose
		LTVector		m_vGripPalm;			// model-space palm held while the weapon animates

		// The first-person model's left arm follows the left controller: a node control moves the
		// arm's root node (the rest of the arm follows it) by m_mArm, in model space.
		LTBOOL			m_bLeftArm;				// avp2xr.ini [VR] LeftArm
		LTVector		m_vLeftOffset;			// LeftHandRight/Up/Forward, world units in the controller's frame
		LTRotation		m_rLeftAngle;			// LeftHandPitch/Yaw/Roll
		LTBOOL			m_bOffWorld;			// m_vOffPos/m_rOffRot are set for this frame
		LTVector		m_vOffPos;				// the off-hand controller's grip pose (palm) in the world
		LTRotation		m_rOffRot;				// its frame for the hand, like m_rAimRot for the right hand (before the angle)
		LTBOOL			m_bArmForcePose;		// avp2xr.ini LeftArmForcePose (diagnostic): pose the idle model each frame
		LTBOOL			m_bLeftMatch;			// avp2xr.ini LeftHandMatch: turn the hand to mirror the right one
		char			m_szIni[MAX_PATH];		// avp2xr.ini, read again for [LeftHand] on each weapon change
		LTVector		m_vArmOffset;			// this weapon's left hand offset and angle ([LeftHand] or the defaults)
		LTRotation		m_rArmAngle;
		LTFLOAT			m_fTwoHandDown;			// avp2xr.ini TwoHandedHandDown, world units: the left hand goes this far
												// below the gun while two-handed, so it doesn't sit in the barrel
		LTFLOAT			m_fTwoHandBlend;		// 0-1, eases the hand down and back up
		LTFLOAT			m_fArmBlend;			// 0-1: the arm is the animation's (0, while switching weapons)
												// or the controller's (1), eased between
		LTRotation		m_rGripToAim;			// the right controller's aim pose relative to its grip pose
		HOBJECT			m_hArmModel;			// the model the node control is on
		struct WEAPON	*m_pArmWeapon;			// ...and the weapon it was set up for
		HMODELNODE		m_hArmRoot;				// the left arm's root node (INVALID_MODEL_NODE = none)
		HMODELNODE		m_hArmHand[4];			// left wrist, middle, pointer and pinky knuckles (INVALID_MODEL_NODE = none)
		HMODELNODE		m_hArmRef;				// the model's root node, which the arm's move leaves alone
		LTBOOL			m_bArmActive;			// apply m_mArm when the model is posed. It stays on between frames:
												// the engine poses the model once a frame, during the game's
												// update (at the game's placement), and draws that pose.
		uint32			m_nArmTime;				// GetTickCount() m_mArm was last worked out
		LTBOOL			m_bArmDrawing;			// between PlaceLeftArm and RestoreWeapon (the eye renders)
		int				m_nArmPasses[2];		// node control passes since the last log: otherwise, while drawing
		int				m_nArmNudge;			// +1/-1: which way the animation time was last nudged
		int				m_nArmForced;			// -1 = not checked for this weapon yet, else whether forcing a pose worked
		LTBOOL			m_bArmApplied;			// m_mArm was applied to the root in this pass over the nodes
		LTMatrix		m_mArm;					// the arm's move in model space
		LTMatrix		m_mArmInv;
		LTMatrix		m_mArmRefInv;			// inverse of the reference node's model-space transform
		// The node control's matrices are in the space the model is being posed in (its placement
		// included), found each pass from the reference node: model space to that space and back,
		// and m_mArm in that space
		LTBOOL			m_bArmPass;
		LTMatrix		m_mArmPass;
		LTMatrix		m_mArmPassInv;
		LTMatrix		m_mArmPassMove;
		LTBOOL			m_bArmSeen;				// the node control has seen the wrist since the weapon changed
		LTVector		m_vArmSeen[4];			// ...as animated (model space, before m_mArm)

		// Models whose left arm is in pieces of its own draw it as a second copy of the model placed at
		// the left controller (so it's lit from its own side), with those pieces hidden on the weapon
		// model, instead of moving the arm's nodes
		HOBJECT			m_hArmCopy;				// the copy (LTNULL = none)
		int				m_nArmSplit;			// the weapon model's entry in the piece table, -1 = not split
		LTBOOL			m_bArmCopyPieces;		// the pieces are set up on the copy and the weapon model
		HMODELPIECE		m_hArmMainPieces[8];	// the weapon model's left-arm pieces
		int				m_nArmMainPieces;
		LTBOOL			m_bArmCopyActive;		// the copy is drawn this frame, at:
		LTVector		m_vArmCopyOrigin;		// (world, full size, like m_vWeaponOrigin)
		LTRotation		m_rArmCopyRot;
		// The copy takes the weapon model's pose as the engine works it out (blends between animations
		// included; the copy's own animation can't blend the same way): each node relative to the
		// model's root node, recorded by WeaponPoseNodeControl and applied by ArmCopyNodeControl
		enum { MAX_POSE_NODES = 256 };
		LTMatrix		m_mPose[MAX_POSE_NODES];
		LTBOOL			m_bPose[MAX_POSE_NODES];
		LTMatrix		m_mPoseRefInv;			// the weapon's root node in this pass, inverted
		LTBOOL			m_bPoseRef;
		LTMatrix		m_mCopyRef;				// the copy's root node in its pass
		LTBOOL			m_bCopyRef;
		LTBOOL			m_bArmHeld;				// m_vArmPalm and the hand frames are from the idle pose
		LTVector		m_vArmPalm;				// model-space left palm held while the weapon animates
		LTBOOL			m_bArmMatch;			// the frames below are set: the hand turns to mirror the right
		LTRotation		m_rArmHand;				// the left hand's frame as animated (model space)
		LTRotation		m_rArmMirror;			// the right hand's frame mirrored, for the left one
		LTFLOAT			m_fTanHalfX;
		LTFLOAT			m_fTanHalfY;
		uint32			m_nScreenWidth;
		uint32			m_nScreenHeight;
		LARGE_INTEGER	m_Prof[PROF_COUNT];		// this frame's marks
		LARGE_INTEGER	m_ProfLastPre;			// the last stereo frame's PROF_PREUPDATE (0 = none)
		LARGE_INTEGER	m_ProfLastPost;			// ...and its PROF_POSTUPDATE
		double			m_fProfSum[10];			// ms, see ProfileMark
		double			m_fProfMaxFrame;
		int				m_nProfFrames;
		uint32			m_nProfStart;
};

// ----------------------------------------------------------------------- //

// Appends a line to %LOCALAPPDATA%\avp2xr\cshell.log (diagnostics for the VR build).
void VRLog(const char *szFormat, ...);

// ----------------------------------------------------------------------- //

#endif
