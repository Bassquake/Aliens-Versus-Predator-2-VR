// ----------------------------------------------------------------------- //
//
// MODULE  : CrosshairMgr.h
//
// PURPOSE : Implementation of CrosshairMgr class
//
// CREATED : 8/28/00
//
// (c) 1997-2000 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //
 
#include "stdafx.h"
#include "CrosshairMgr.h"
#include "GameClientShell.h"
#include "CharacterFuncs.h"
#include "VarTrack.h"


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CCrosshairMgr::CCrosshairMgr()
//
//	PURPOSE:	Constructor
//
// ----------------------------------------------------------------------- //

CCrosshairMgr::CCrosshairMgr()
{
	// common members
	m_bCrosshairEnabled		= DTRUE;
	m_bCrosshairOverride	= LTTRUE;
	m_bVRAim				= LTFALSE;
	m_bVRToolAim			= LTFALSE;
	m_bUseActivate			= LTFALSE;
	
	// activate crosshair members
	m_hActivateCrosshair	= LTNULL;
	m_szActImage			= "";
	m_nHalfActXhairWidth	= 0;
	m_nHalfActXhairHeight	= 0;
	m_eActMode				= XHM_TARGETING;
	m_fActScale				= 1.0f;

	// targeting crosshair members
	m_hCrosshair			= LTNULL;
	m_szCrosshairImage[0]	= 0;
	m_fCrosshairAlpha		= 1.0f;
	m_nSolidRGB				= 0xFFFFFF;
	m_ptCHPos				= LTIntPt(0,0);
	m_bCustomCrosshairPos	= LTFALSE;
	m_nHalfXhairWidth		= 0;	
	m_nHalfXhairHeight		= 0;
	m_fScale				= 1.0f;
	m_hCursorColor			= LTNULL;
	m_bDrawSolid			= LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CCrosshairMgr::~CCrosshairMgr()
//
//	PURPOSE:	Destructor
//
// ----------------------------------------------------------------------- //

CCrosshairMgr::~CCrosshairMgr()
{
	Term();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CCrosshairMgr::Init()
//
//	PURPOSE:	Initialize
//
// ----------------------------------------------------------------------- //

LTBOOL CCrosshairMgr::Init()
{
	//load up the activate and targeting crosshairs
	SetActivateCrosshair(XHM_DEFAULT_ACTIVATE);
	LoadCrosshair("Interface\\StatusBar\\Marine\\Xhair_normal.pcx", 4.0f);

	return DTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CCrosshairMgr::Term()
//
//	PURPOSE:	Terminate the crosshair stuff
//
// ----------------------------------------------------------------------- //

// ----------------------------------------------------------------------- //
// Smoothed crosshair images for VR, by image file (the shared surfaces can be freed and their
// handles reused). The images are tiny (17 x 17 for the smartgun's), so drawn at the headset's
// resolution each pixel shows as a block. Each is scaled up 4x with Scale2x (EPX) done twice, a
// pixel-art scaler: curves and diagonals come out smooth, straight lines stay sharp, and no new
// colours appear (so black stays the transparent key).

#define SMOOTH_MAX		32

struct SmoothImage
{
	char		szName[128];
	uint32		nTint;				// RGB every visible pixel is drawn in (the solid-colour crosshairs), 0 = its own
	HSURFACE	hSurface;			// the smoothed copy, made by GetSmoothImage (LTNULL until then, or if it failed)
	LTBOOL		bTriedSurface;		// ...which it has tried
	uint32		nWidth, nHeight;	// the original's size
	uint32		*pRaw;				// the original's pixels as 0xAARRGGBB (black see-through), for avp2xr
	uint32		*pSmooth;			// ...and the smoothed copy's (4x the size)
	uint32		nId;				// unique to this entry
};
static SmoothImage s_SmoothImages[SMOOTH_MAX];
static int s_nSmoothImages = 0;

static void Scale2x(const uint32 *pSrc, uint32 w, uint32 h, uint32 *pDst)
{
	for(uint32 y = 0; y < h; y++)
		for(uint32 x = 0; x < w; x++)
		{
			uint32 P = pSrc[y * w + x];
			uint32 A = pSrc[(y > 0 ? y - 1 : y) * w + x];			// above
			uint32 D = pSrc[(y + 1 < h ? y + 1 : y) * w + x];		// below
			uint32 C = pSrc[y * w + (x > 0 ? x - 1 : x)];			// left
			uint32 B = pSrc[y * w + (x + 1 < w ? x + 1 : x)];		// right
			uint32 *pOut = pDst + (y * 2) * (w * 2) + x * 2;
			pOut[0] = (C == A && C != D && A != B) ? A : P;
			pOut[1] = (A == B && A != C && B != D) ? B : P;
			pOut[w * 2] = (D == C && D != B && C != A) ? C : P;
			pOut[w * 2 + 1] = (B == D && B != A && D != C) ? D : P;
		}
}

static uint32 s_nNextSmoothId = 1;

static uint32 *WithAlpha(const uint32 *p, uint32 n)
{
	uint32 *pOut = new uint32[n];
	for(uint32 i = 0; i < n; i++)
		pOut[i] = p[i] ? (p[i] | 0xFF000000) : 0;
	return pOut;
}

// The pixels: the original's and the smoothed copy's, as 0xAARRGGBB (what avp2xr takes)
static void BuildSmoothPixels(HSURFACE hSrc, uint32 w, uint32 h, uint32 nTint, SmoothImage &img)
{
	uint32 nStart = GetTickCount();
	uint32 *p1 = new uint32[w * h];
	uint32 *p2 = new uint32[w * h * 4];
	uint32 *p4 = new uint32[w * h * 16];
	for(uint32 y = 0; y < h; y++)
		for(uint32 x = 0; x < w; x++)
		{
			HLTCOLOR c = 0;
			g_pLTClient->GetPixel(hSrc, x, y, &c);
			p1[y * w + x] = c & 0xFFFFFF;
			if(nTint && p1[y * w + x])
				p1[y * w + x] = nTint;
		}
	Scale2x(p1, w, h, p2);
	Scale2x(p2, w * 2, h * 2, p4);
	img.pRaw = WithAlpha(p1, w * h);
	img.pSmooth = WithAlpha(p4, w * h * 16);
	delete[] p1;
	delete[] p2;
	delete[] p4;
	VRLog("Smoothed image %s (%ux%u): %u ms", img.szName, w, h, GetTickCount() - nStart);
}

// The smoothed copy as a surface, for the game to draw (without avp2xr 10's crosshair). Made only
// when that's wanted: a SetPixel per pixel is slow (about 1.2 s for a 128 x 128 image, 16 times the pixels)
static HSURFACE BuildSmoothSurface(const SmoothImage &img)
{
	uint32 w = img.nWidth * 4, h = img.nHeight * 4;
	HSURFACE hDst = g_pLTClient->CreateSurface(w, h);
	if(!hDst)
		return LTNULL;
	uint32 nStart = GetTickCount();
	for(uint32 Y = 0; Y < h; Y++)
		for(uint32 X = 0; X < w; X++)
			g_pLTClient->SetPixel(hDst, X, Y, img.pSmooth[Y * w + X] & 0xFFFFFF);
	g_pLTClient->OptimizeSurface(hDst, SETRGB_T(0, 0, 0));
	VRLog("Smoothed surface %s (%ux%u): %u ms", img.szName, w, h, GetTickCount() - nStart);
	return hDst;
}

static SmoothImage *FindSmoothImage(const char *szName, HSURFACE hSrc, uint32 nTint)
{
	if(!szName || !szName[0] || !hSrc)
		return LTNULL;
	uint32 w = 0, h = 0;
	g_pLTClient->GetSurfaceDims(hSrc, &w, &h);
	if(!w || !h || w > 256 || h > 256)
		return LTNULL;
	for(int i = 0; i < s_nSmoothImages; i++)
		if(!stricmp(s_SmoothImages[i].szName, szName) && s_SmoothImages[i].nTint == nTint &&
		   s_SmoothImages[i].nWidth == w && s_SmoothImages[i].nHeight == h)
			return &s_SmoothImages[i];
	if(s_nSmoothImages >= SMOOTH_MAX)
		return LTNULL;
	SmoothImage &img = s_SmoothImages[s_nSmoothImages++];
	strncpy(img.szName, szName, sizeof(img.szName) - 1);
	img.szName[sizeof(img.szName) - 1] = 0;
	img.nWidth = w;
	img.nHeight = h;
	img.nTint = nTint;
	img.pRaw = img.pSmooth = LTNULL;
	img.hSurface = LTNULL;
	img.bTriedSurface = LTFALSE;
	img.nId = s_nNextSmoothId++;
	BuildSmoothPixels(hSrc, w, h, nTint, img);
	return &img;
}

static HSURFACE GetSmoothImage(const char *szName, HSURFACE hSrc, uint32 nTint)
{
	SmoothImage *pImg = FindSmoothImage(szName, hSrc, nTint);
	if(pImg && !pImg->bTriedSurface)
	{
		pImg->bTriedSurface = LTTRUE;
		pImg->hSurface = BuildSmoothSurface(*pImg);
	}
	return pImg && pImg->hSurface ? pImg->hSurface : hSrc;
}

void CCrosshairMgr::Term()
{
	if (!g_pLTClient) return;

	for(int i = 0; i < s_nSmoothImages; i++)
	{
		if(s_SmoothImages[i].hSurface)
			g_pLTClient->DeleteSurface(s_SmoothImages[i].hSurface);
		delete[] s_SmoothImages[i].pRaw;
		delete[] s_SmoothImages[i].pSmooth;
	}
	s_nSmoothImages = 0;

//	if (m_hCrosshair)
//	{
//		g_pLTClient->DeleteSurface (m_hCrosshair);
//		m_hCrosshair = LTNULL;
//	}

//	if (m_hActivateCrosshair)
//	{
//		g_pLTClient->DeleteSurface (m_hActivateCrosshair);
//		m_hActivateCrosshair = LTNULL;
//	}
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CCrosshairMgr::Save
//
//	PURPOSE:	Save the player stats info
//
// --------------------------------------------------------------------------- //

void CCrosshairMgr::Save(HMESSAGEWRITE hWrite)
{
	if (!g_pLTClient || !g_pWeaponMgr) return;

	g_pLTClient->WriteToMessageByte(hWrite, m_bCrosshairEnabled);
	g_pLTClient->WriteToMessageByte(hWrite, m_bCrosshairOverride);
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CCrosshairMgr::Load
//
//	PURPOSE:	Load the player stats info
//
// --------------------------------------------------------------------------- //

void CCrosshairMgr::Load(HMESSAGEREAD hRead)
{
	if (!g_pLTClient || !g_pWeaponMgr) return;

	m_bCrosshairEnabled		= (LTBOOL) g_pLTClient->ReadFromMessageByte(hRead);
	m_bCrosshairOverride	= (LTBOOL) g_pLTClient->ReadFromMessageByte(hRead);
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CCrosshairMgr::DrawCrosshair
//
//	PURPOSE:	Draw the crosshair
//
// --------------------------------------------------------------------------- //

void CCrosshairMgr::DrawCrosshair()
{
	if (!g_pLTClient || !m_bCrosshairEnabled || !m_bCrosshairOverride) return;


	if(m_bUseActivate)
	{
		// For the torch and hacking device, see that we are not firing...
		if(m_eActMode == XHM_MARINE_WELD || m_eActMode == XHM_MARINE_CUT)
		{
			// Check the weapon...
			WEAPON* pWep = g_pGameClientShell->GetWeaponModel()->GetWeapon();

			if(pWep && (stricmp("Blowtorch", pWep->szName)==0))
			{
				uint32 dwFlags = g_pGameClientShell->GetPlayerMovement()->GetControlFlags();

				// Hide the cursor if we are firing...
				if(dwFlags & CM_FLAG_PRIMEFIRING)
					return;
			}
		}
		else if(m_eActMode == XHM_MARINE_HACK)
		{
			// Check the weapon...
			WEAPON* pWep = g_pGameClientShell->GetWeaponModel()->GetWeapon();

			if(pWep && ((stricmp("MarineHackingDevice", pWep->szName)==0) || (stricmp("PredatorHackingDevice", pWep->szName)==0)) )
			{
				uint32 dwFlags = g_pGameClientShell->GetPlayerMovement()->GetControlFlags();

				// Hide the cursor if we are firing...
				if(dwFlags & CM_FLAG_PRIMEFIRING)
					return;
			}
		}

		// In VR a tool's crosshair is drawn at where the tool points (VRMgr), not here
		if(!(m_bVRAim && m_bVRToolAim))
			DrawActivateCrosshair();
	}
	else if(!m_bVRAim)
	{
		DrawTargetingCrosshair();
	}
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CCrosshairMgr::WantsVRCrosshair / DrawCrosshairAt
//
//	PURPOSE:	VR: the targeting crosshair drawn in an eye's view at the aim point
//
// --------------------------------------------------------------------------- //

LTBOOL CCrosshairMgr::WantsVRCrosshair() const
{
	if(!g_pLTClient || !m_bCrosshairEnabled || !m_bCrosshairOverride)
		return LTFALSE;
	if(m_bUseActivate)
		return m_bVRToolAim && m_hActivateCrosshair;
	return m_hCrosshair != LTNULL;
}

LTBOOL CCrosshairMgr::GetVRCrosshairImage(LTBOOL bSmooth, const uint32 *&pPixels, uint32 &nWidth, uint32 &nHeight, uint32 &nId,
										  LTFLOAT &fHalfW, LTFLOAT &fHalfH, LTFLOAT &fAlpha) const
{
	if(!WantsVRCrosshair())
		return LTFALSE;

	SmoothImage *pImg;
	if(m_bUseActivate)
	{
		// A tool's crosshair is hidden while the tool is working, as on the screen
		if((m_eActMode == XHM_MARINE_WELD || m_eActMode == XHM_MARINE_CUT || m_eActMode == XHM_MARINE_HACK) &&
		   (g_pGameClientShell->GetPlayerMovement()->GetControlFlags() & CM_FLAG_PRIMEFIRING))
			return LTFALSE;
		pImg = FindSmoothImage(m_szActImage, m_hActivateCrosshair, 0);
		fHalfW = m_nHalfActXhairWidth * m_fActScale;
		fHalfH = m_nHalfActXhairHeight * m_fActScale;
		fAlpha = 1.0f;
	}
	else
	{
		if(m_fScale <= 0.0f)
			return LTFALSE;	// hidden (the railgun's zoom)
		pImg = FindSmoothImage(m_szCrosshairImage, m_hCrosshair, m_bDrawSolid ? m_nSolidRGB : 0);
		fHalfW = m_nHalfXhairWidth * m_fScale;
		fHalfH = m_nHalfXhairHeight * m_fScale;
		fAlpha = m_fCrosshairAlpha;
	}
	if(!pImg || !pImg->pRaw || !pImg->pSmooth)
		return LTFALSE;

	pPixels = bSmooth ? pImg->pSmooth : pImg->pRaw;
	nWidth = bSmooth ? pImg->nWidth * 4 : pImg->nWidth;
	nHeight = bSmooth ? pImg->nHeight * 4 : pImg->nHeight;
	nId = pImg->nId * 2 + (bSmooth ? 1 : 0);
	return LTTRUE;
}

LTBOOL CCrosshairMgr::GetVRImage(const char *szName, HSURFACE hSurface, LTBOOL bSmooth, const uint32 *&pPixels,
								 uint32 &nWidth, uint32 &nHeight, uint32 &nId)
{
	SmoothImage *pImg = FindSmoothImage(szName, hSurface, 0);
	if(!pImg || !pImg->pRaw || !pImg->pSmooth)
		return LTFALSE;

	pPixels = bSmooth ? pImg->pSmooth : pImg->pRaw;
	nWidth = bSmooth ? pImg->nWidth * 4 : pImg->nWidth;
	nHeight = bSmooth ? pImg->nHeight * 4 : pImg->nHeight;
	nId = pImg->nId * 2 + (bSmooth ? 1 : 0);
	return LTTRUE;
}

void CCrosshairMgr::DrawCrosshairAt(int cx, int cy, LTFLOAT fScaleX, LTFLOAT fScaleY, LTBOOL bSmooth)
{
	if(!WantsVRCrosshair())
		return;

	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	HDECOLOR hTransColor = SETRGB_T(0,0,0);

	// A tool's activate crosshair (hack, weld, cut), hidden while the tool is working, as on the screen
	if(m_bUseActivate)
	{
		if((m_eActMode == XHM_MARINE_WELD || m_eActMode == XHM_MARINE_CUT || m_eActMode == XHM_MARINE_HACK) &&
		   (g_pGameClientShell->GetPlayerMovement()->GetControlFlags() & CM_FLAG_PRIMEFIRING))
			return;

		int nActW = (int)(m_nHalfActXhairWidth * m_fActScale * fScaleX + 0.5f);
		int nActH = (int)(m_nHalfActXhairHeight * m_fActScale * fScaleY + 0.5f);
		if(nActW < 1) nActW = 1;
		if(nActH < 1) nActH = 1;
		LTRect rcAct;
		rcAct.left = cx - nActW;
		rcAct.top = cy - nActH;
		rcAct.right = cx + nActW;
		rcAct.bottom = cy + nActH;
		HSURFACE hAct = bSmooth ? GetSmoothImage(m_szActImage, m_hActivateCrosshair, 0) : m_hActivateCrosshair;
		g_pLTClient->ScaleSurfaceToSurfaceTransparent(hScreen, hAct, &rcAct, LTNULL, hTransColor);
		return;
	}

	if(m_fScale <= 0.0f)
		return;	// hidden (the railgun's zoom)
	int nHalfW = (int)(m_nHalfXhairWidth * m_fScale * fScaleX + 0.5f);
	int nHalfH = (int)(m_nHalfXhairHeight * m_fScale * fScaleY + 0.5f);
	if(nHalfW < 1) nHalfW = 1;
	if(nHalfH < 1) nHalfH = 1;

	LTRect rcDest;
	rcDest.left = cx - nHalfW;
	rcDest.top = cy - nHalfH;
	rcDest.right = cx + nHalfW;
	rcDest.bottom = cy + nHalfH;

	// The solid-colour crosshairs (the smartgun's, the SADAR's while targeting) get a smoothed copy
	// in that colour, drawn like the others: the solid-colour draw doesn't go at the headset's
	// resolution, so it came out blocky and too big.
	HSURFACE hImage = m_hCrosshair;
	if(bSmooth)
	{
		hImage = GetSmoothImage(m_szCrosshairImage, m_hCrosshair, m_bDrawSolid ? m_nSolidRGB : 0);
		if(hImage != m_hCrosshair)
		{
			g_pLTClient->SetSurfaceAlpha(hImage, m_fCrosshairAlpha);
			g_pLTClient->ScaleSurfaceToSurfaceTransparent(hScreen, hImage, &rcDest, LTNULL, hTransColor);
			return;
		}
	}
	if(!m_bDrawSolid)
		g_pLTClient->ScaleSurfaceToSurfaceTransparent(hScreen, hImage, &rcDest, LTNULL, hTransColor);
	else
		g_pLTClient->ScaleSurfaceToSurfaceSolidColor(hScreen, hImage, &rcDest, LTNULL, hTransColor, m_hCursorColor);
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CCrosshairMgr::DrawActivateCrosshair
//
//	PURPOSE:	Draw the activate crosshair
//
// --------------------------------------------------------------------------- //

void CCrosshairMgr::DrawActivateCrosshair()
{
	//get screen dims
	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	uint32 nHalfScreenWidth, nHalfScreenHeight;
	g_pLTClient->GetSurfaceDims(hScreen, &nHalfScreenWidth, &nHalfScreenHeight);
	nHalfScreenWidth>>=1;
	nHalfScreenHeight>>=1;

	//draw the crosshair to the screen
	HDECOLOR hTransColor = SETRGB_T(0,0,0);

	LTRect rcDest;
	rcDest.top = nHalfScreenHeight-(uint32)(m_nHalfActXhairHeight*m_fActScale);
	rcDest.left = nHalfScreenWidth-(uint32)(m_nHalfActXhairWidth*m_fActScale);
	rcDest.bottom = rcDest.top + (uint32)(m_nHalfActXhairHeight*m_fActScale*2);
	rcDest.right = rcDest.left + (uint32)(m_nHalfActXhairWidth*m_fActScale*2);

	g_pLTClient->ScaleSurfaceToSurfaceTransparent(	hScreen, 
													m_hActivateCrosshair,
													&rcDest, 
													LTNULL, 
													hTransColor);
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CCrosshairMgr::DrawTargatingCrosshair
//
//	PURPOSE:	Draw the targeting crosshair
//
// --------------------------------------------------------------------------- //

void CCrosshairMgr::DrawTargetingCrosshair()
{
	//get screen dims
	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	uint32 nHalfScreenWidth, nHalfScreenHeight;
	g_pLTClient->GetSurfaceDims(hScreen, &nHalfScreenWidth, &nHalfScreenHeight);
	nHalfScreenWidth>>=1;
	nHalfScreenHeight>>=1;

	//find the center of the crosshair dest
	int cx = m_bCustomCrosshairPos?m_ptCHPos.x:nHalfScreenWidth;
	int cy = m_bCustomCrosshairPos?m_ptCHPos.y:nHalfScreenHeight;


	//draw the crosshair to the screen
	HDECOLOR hTransColor = SETRGB_T(0,0,0);

	LTRect rcDest;
	rcDest.top = cy-(uint32)(m_nHalfXhairHeight*m_fScale);
	rcDest.left = cx-(uint32)(m_nHalfXhairWidth*m_fScale);
	rcDest.bottom = rcDest.top + (uint32)(m_nHalfXhairHeight*m_fScale*2);
	rcDest.right = rcDest.left + (uint32)(m_nHalfXhairWidth*m_fScale*2);

	if(!m_bDrawSolid)
	{
		if(m_fScale != 1.0f)
			g_pLTClient->ScaleSurfaceToSurfaceTransparent(	hScreen, 
															m_hCrosshair,
															&rcDest, 
															LTNULL, 
															hTransColor);
		else
			g_pLTClient->DrawSurfaceToSurfaceTransparent(	hScreen, 
															m_hCrosshair,
															LTNULL,
															rcDest.left,
															rcDest.top, 
															hTransColor);
	}
	else
	{
		if(m_fScale != 1.0f)
			g_pLTClient->ScaleSurfaceToSurfaceSolidColor(	hScreen, 
															m_hCrosshair,
															&rcDest, 
															LTNULL, 
															hTransColor, 
															m_hCursorColor);
		else
			g_pLTClient->DrawSurfaceSolidColor(	hScreen, 
												m_hCrosshair,
												LTNULL, 
												rcDest.left,
												rcDest.top, 
												hTransColor, 
												m_hCursorColor);
	}
}

// ----------------------------------------------------------------------- //
//
// FUNCTION:	CCrosshairMgr::SetCrosshairColors()
//
// PURPOSE:		Sets the colors and alpha of the crosshair
//
// ----------------------------------------------------------------------- //

void CCrosshairMgr::SetCrosshairColors(LTFLOAT fRed, LTFLOAT fGreen, LTFLOAT fBlue, LTFLOAT fAlpha)
{
	m_bDrawSolid = LTTRUE;
	m_hCursorColor = g_pLTClient->SetupColor1(fRed,fGreen,fBlue,LTTRUE);
	g_pLTClient->SetSurfaceAlpha(m_hCrosshair,fAlpha);
	int r = (int)(fRed * 255.0f + 0.5f), g = (int)(fGreen * 255.0f + 0.5f), b = (int)(fBlue * 255.0f + 0.5f);
	if(r + g + b < 24)
		r = g = b = 8;	// not the transparent black
	m_nSolidRGB = SETRGB(r, g, b);
	m_fCrosshairAlpha = fAlpha;
}

// ----------------------------------------------------------------------- //
//
// FUNCTION:	CCrosshairMgr::SetCrosshairColors()
//
// PURPOSE:		Sets the colors and alpha of the crosshair
//
// ----------------------------------------------------------------------- //

void CCrosshairMgr::UpdateCrosshairColors(LTFLOAT fRed, LTFLOAT fGreen, LTFLOAT fBlue, LTFLOAT fAlpha)
{
	SetCrosshairColors(fRed, fGreen, fBlue, fAlpha);
}

// ----------------------------------------------------------------------- //
//
// FUNCTION:	CCrosshairMgr::EnableCrosshair()
//
// PURPOSE:		Enable croshairs
//
// ----------------------------------------------------------------------- //

void CCrosshairMgr::EnableCrosshair(LTBOOL b)
{ 
	m_bCrosshairEnabled = b; 
}

// ----------------------------------------------------------------------- //
//
// FUNCTION:	CCrosshairMgr::LoadCrosshair()
//
// PURPOSE:		Load a new croshair
//
// ----------------------------------------------------------------------- //

void CCrosshairMgr::LoadCrosshair(char* szImage, LTFLOAT fScale, LTFLOAT fAlpha)
{
	if(szImage && strlen(szImage) > 0)
	{
//		if(m_hCrosshair)
//		{
//			g_pLTClient->DeleteSurface (m_hCrosshair);
//			m_hCrosshair		= LTNULL;
//		}

		m_bDrawSolid = LTFALSE;

		m_hCrosshair = g_pInterfaceResMgr->GetSharedSurface(szImage);
		g_pLTClient->SetSurfaceAlpha (m_hCrosshair, fAlpha);
		strncpy(m_szCrosshairImage, szImage, sizeof(m_szCrosshairImage) - 1);
		m_szCrosshairImage[sizeof(m_szCrosshairImage) - 1] = 0;
		m_fCrosshairAlpha = fAlpha;
		g_pLTClient->GetSurfaceDims(m_hCrosshair, &m_nHalfXhairWidth, &m_nHalfXhairHeight);
		m_nHalfXhairWidth>>=1;
		m_nHalfXhairHeight>>=1;
		m_fScale = fScale;
		m_bCustomCrosshairPos	= LTFALSE;
	}
}

// ----------------------------------------------------------------------- //
//
// FUNCTION:	CCrosshairMgr::SetActivateCrosshair()
//
// PURPOSE:		Load a new croshair
//
// ----------------------------------------------------------------------- //

void CCrosshairMgr::SetActivateCrosshair(XHairMode eMode)
{
	//clean up the old crosshair
//	if(m_hActivateCrosshair)
//	{
//		g_pLTClient->DeleteSurface (m_hActivateCrosshair);
//		m_hActivateCrosshair = LTNULL;
//	}

	//load the name and scale
	switch (m_eActMode)
	{
	case (XHM_DEFAULT_ACTIVATE):
		m_szActImage = "Interface\\StatusBar\\Marine\\Xhair_activate.pcx";
		m_hActivateCrosshair = g_pInterfaceResMgr->GetSharedSurface("Interface\\StatusBar\\Marine\\Xhair_activate.pcx");
		m_fActScale = 1.0f;
		break;
	case (XHM_MARINE_WELD):
		m_szActImage = "Interface\\StatusBar\\Marine\\Xhair_weld.pcx";
		m_hActivateCrosshair = g_pInterfaceResMgr->GetSharedSurface("Interface\\StatusBar\\Marine\\Xhair_weld.pcx");
		m_fActScale = 1.0f;
		break;
	case (XHM_MARINE_CUT):
		m_szActImage = "Interface\\StatusBar\\Marine\\Xhair_cut.pcx";
		m_hActivateCrosshair = g_pInterfaceResMgr->GetSharedSurface("Interface\\StatusBar\\Marine\\Xhair_cut.pcx");
		m_fActScale = 1.0f;
		break;
	case (XHM_MARINE_HACK):
		if(IsPredator(g_pGameClientShell->GetPlayerMovement()->GetCharacterButes()))
			m_szActImage = "Interface\\StatusBar\\Predator\\xhair_predhack.pcx";
		else
			m_szActImage = "Interface\\StatusBar\\Marine\\Xhair_hack.pcx";
		m_hActivateCrosshair = g_pInterfaceResMgr->GetSharedSurface((char*)m_szActImage);
		m_fActScale = 1.0f;
		break;
	case (XHM_PRED_DETONATE):
		m_szActImage = "Interface\\StatusBar\\predator\\Xhair_predbomb.pcx";
		m_hActivateCrosshair = g_pInterfaceResMgr->GetSharedSurface("Interface\\StatusBar\\predator\\Xhair_predbomb.pcx");
		m_fActScale = 1.0f;
		break;
	default: return;
	}

	//finish the setup here
	g_pLTClient->SetSurfaceAlpha (m_hActivateCrosshair, 1.0f);
	g_pLTClient->GetSurfaceDims(m_hActivateCrosshair, &m_nHalfActXhairWidth, &m_nHalfActXhairHeight);
	m_nHalfActXhairWidth>>=1;
	m_nHalfActXhairHeight>>=1;
}

// ----------------------------------------------------------------------- //
//
// FUNCTION:	CCrosshairMgr::SetActivateMode()
//
// PURPOSE:		Load a new croshair
//
// ----------------------------------------------------------------------- //

void CCrosshairMgr::SetActivateMode(XHairMode eMode)
{ 
	if(m_eActMode == eMode) return;
	
	//record our new mode
	m_eActMode = eMode;

	m_bUseActivate = eMode==XHM_TARGETING?LTFALSE:LTTRUE;

	if(m_bUseActivate)
		SetActivateCrosshair(eMode); 
}