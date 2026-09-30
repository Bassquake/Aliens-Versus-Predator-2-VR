// ----------------------------------------------------------------------- //
//
// MODULE  : CrosshairMgr.h
//
// PURPOSE : Definition of CrosshairMgr class
//
// CREATED : 8/28/00
//
// (c) 1997-2000 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //
 
#ifndef __CROSSHAIRMGR_H
#define __CROSSHAIRMGR_H

#include "basedefs_de.h"

//activate crosshair modes
enum XHairMode
{
	XHM_TARGETING = 0,
	XHM_DEFAULT_ACTIVATE,
	XHM_MARINE_WELD,
	XHM_MARINE_CUT,
	XHM_MARINE_HACK,
	XHM_PRED_DETONATE,
};

class CCrosshairMgr
{
public:

	CCrosshairMgr();
	~CCrosshairMgr();

	LTBOOL		Init();
	void		Term();

	void		Save(HMESSAGEWRITE hWrite);
	void		Load(HMESSAGEREAD hRead);

	void		ToggleCrosshairOverride(){ m_bCrosshairOverride = !m_bCrosshairOverride; }
	void		ToggleCrosshair(){ m_bCrosshairEnabled = !m_bCrosshairEnabled; }
	void		EnableCrosshair(LTBOOL b=LTTRUE);
	LTBOOL		CrosshairEnabled() const { return m_bCrosshairEnabled; }
	LTBOOL		CrosshairOn() const { return m_bCrosshairEnabled; }

	void		DrawCrosshair();

	void		UpdateCrosshairColors(LTFLOAT fRed = 1.0f, LTFLOAT fGreen = 1.0f, LTFLOAT fBlue = 1.0f, LTFLOAT fAlpha = 1.0f);
	void		SetCrosshairColors(LTFLOAT fRed, LTFLOAT fGreen, LTFLOAT fBlue, LTFLOAT fAlpha);
	void		SetCustomCHPosOn(LTBOOL bOn)	{ m_bCustomCrosshairPos = bOn; }
	void		SetCustomCHPos(LTIntPt pt)		{ m_ptCHPos = pt; }
	void		LoadCrosshair(char* szImage, LTFLOAT fScale, LTFLOAT fAlpha=1.0f);
	void		SetActivateCrosshairOn(LTBOOL bOn) { m_bUseActivate=bOn; }
	void		SetActivateMode(XHairMode eMode);

	// VR: with controller aiming the targeting crosshair isn't drawn on the screen (the head-locked
	// HUD) but by VRMgr in each eye at the aim point, through DrawCrosshairAt, at centre (cx, cy)
	// with its size scaled by fScaleX / fScaleY on top of its own scale.
	void		SetVRAim(LTBOOL bOn)			{ m_bVRAim = bOn; }
	// VR: a tool (hacking device, torch) is in hand and its activate crosshair follows where it's
	// pointed, so it's drawn at the aim point like the targeting one rather than on the HUD
	void		SetVRToolAim(LTBOOL bOn)		{ m_bVRToolAim = bOn; }
	// bSmooth: draw a smoothed copy of the image (VR: the image is drawn at the headset's
	// resolution, point sampled, so each of its pixels would show as a block)
	LTBOOL		WantsVRCrosshair() const;
	void		DrawCrosshairAt(int cx, int cy, LTFLOAT fScaleX, LTFLOAT fScaleY, LTBOOL bSmooth = LTFALSE);
	// ...or, for avp2xr to draw at the headset's resolution: the image (0xAARRGGBB, alpha 0 =
	// see-through; nId changes when it does), its half size on the flat screen, and its alpha
	LTBOOL		GetVRCrosshairImage(LTBOOL bSmooth, const uint32 *&pPixels, uint32 &nWidth, uint32 &nHeight, uint32 &nId,
									LTFLOAT &fHalfW, LTFLOAT &fHalfH, LTFLOAT &fAlpha) const;

private:

	// private member functions
	void		SetActivateCrosshair(XHairMode eMode);
	void		DrawTargetingCrosshair();
	void		DrawActivateCrosshair();

	// common members
	LTBOOL		m_bCrosshairEnabled;
	LTBOOL		m_bCrosshairOverride;
	LTBOOL		m_bUseActivate;
	
	// activate crosshair members
	HSURFACE	m_hActivateCrosshair;
	uint32		m_nHalfActXhairWidth;
	uint32		m_nHalfActXhairHeight;
	XHairMode	m_eActMode;
	LTFLOAT		m_fActScale;
	const char	*m_szActImage;			// its image file

	// targeting crosshair members
	HSURFACE	m_hCrosshair;
	LTIntPt		m_ptCHPos;
	LTBOOL		m_bCustomCrosshairPos;
	uint32		m_nHalfXhairWidth;
	uint32		m_nHalfXhairHeight;
	LTFLOAT		m_fScale;
	char		m_szCrosshairImage[128];	// its image file
	LTFLOAT		m_fCrosshairAlpha;
	uint32		m_nSolidRGB;				// m_hCursorColor as RGB, for the smoothed copies
	HLTCOLOR	m_hCursorColor;
	LTBOOL		m_bDrawSolid;
	LTBOOL		m_bVRAim;
	LTBOOL		m_bVRToolAim;
};

#endif
