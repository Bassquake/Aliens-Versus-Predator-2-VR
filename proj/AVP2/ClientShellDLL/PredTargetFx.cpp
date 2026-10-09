// ----------------------------------------------------------------------- //
//
// MODULE  : PredTargetFx.cpp
//
// PURPOSE : Predator Target FX - Implementation
//
// CREATED : 6/13/2000
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "PredTargetFx.h"
#include "GameClientShell.h"
#include "VarTrack.h"

extern CGameClientShell* g_pGameClientShell;

VarTrack	g_cvarSegmentDelay;
VarTrack	g_cvarRotRate;
VarTrack	g_cvarSegmentTime;

// The images: the locked triangle, then the targeting ones (base, left, right)
static char s_szLockedImage[] = "Interface\\StatusBar\\Predator\\locked_triangle.pcx";
static char s_szTargetingImage[3][64] = { "Interface\\StatusBar\\Predator\\targeting_triangle3.pcx",
	"Interface\\StatusBar\\Predator\\targeting_triangle1.pcx", "Interface\\StatusBar\\Predator\\targeting_triangle2.pcx" };

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPredTargetSFX::Init
//
//	PURPOSE:	Create the target FX
//
// ----------------------------------------------------------------------- //

DBOOL CPredTargetSFX::Init(HLOCALOBJ hServObj, HMESSAGEREAD hRead)
{
	SFXCREATESTRUCT info;

	info.hServerObj = hServObj;

	return Init(&info);
}

DBOOL CPredTargetSFX::Init(SFXCREATESTRUCT* psfxCreateStruct)
{
	if (!psfxCreateStruct) return DFALSE;

	CSpecialFX::Init(psfxCreateStruct);

	g_cvarSegmentDelay.Init(g_pClientDE, "PredSegmentDelay", NULL, 0.15f);
	g_cvarRotRate.Init(g_pClientDE, "PredRotationRate", NULL, 3.0f);
	g_cvarSegmentTime.Init(g_pClientDE, "PredSegmentTime", NULL, 0.4f);

	return DTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPredTargetSFX::CreateObject
//
//	PURPOSE:	Create object associated with the targeting triangle
//
// ----------------------------------------------------------------------- //

LTBOOL CPredTargetSFX::CreateObject(CClientDE *pClientDE)
{
	if (!CSpecialFX::CreateObject(pClientDE) || !g_pGameClientShell) return DFALSE;

	CSFXMgr* psfxMgr = g_pGameClientShell->GetSFXMgr();
	if (!psfxMgr) return DFALSE;

	// Setup the target triangle...

	ObjectCreateStruct createStruct;
	INIT_OBJECTCREATESTRUCT(createStruct);

	createStruct.m_ObjectType = OT_NORMAL;

	HOBJECT hCamera = g_pGameClientShell->GetCameraMgr()->GetCameraObject(g_pGameClientShell->GetPlayerCamera());
	if (!hCamera) return LTFALSE;

	HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
	if (!hPlayerObj) return LTFALSE;

	LTRotation rRot;
	g_pLTClient->GetObjectRotation(hCamera, &rRot);

	LTVector vPos;
	g_pLTClient->GetObjectPos(hPlayerObj, &vPos);

	createStruct.m_Pos = vPos;
	createStruct.m_Rotation = rRot;

	//create our object
	m_hObject = pClientDE->CreateObject(&createStruct);

	HDECOLOR	hTransColor = SETRGB_T(0,0,0);

	//load up the surfaces
	if(!m_hLockedImage)
	{
		m_hLockedImage = g_pInterfaceResMgr->GetSharedSurface(s_szLockedImage);
		g_pLTClient->SetSurfaceAlpha (m_hLockedImage, 0.5f);
		g_pLTClient->GetSurfaceDims(m_hLockedImage, &m_nHalfImageWidth, &m_nHalfImageHeight);
	}


	if(!m_hTargetingImage[0])
		m_hTargetingImage[0] = g_pInterfaceResMgr->GetSharedSurface(s_szTargetingImage[0]);

	if(!m_hTargetingImage[1])
		m_hTargetingImage[1] = g_pInterfaceResMgr->GetSharedSurface(s_szTargetingImage[1]);

	if(!m_hTargetingImage[2])
		m_hTargetingImage[2] = g_pInterfaceResMgr->GetSharedSurface(s_szTargetingImage[2]);

	for(int i=0 ; i<3 ; i++)
		g_pLTClient->SetSurfaceAlpha (m_hTargetingImage[i], 0.5f);

	//initialize some stuff
	m_fAngle		=	0.0f;

	//get handle to screen surface
	HSURFACE hScreen = g_pClientDE->GetScreenSurface();
	g_pLTClient->GetSurfaceDims(hScreen, &m_nScreenX, &m_nScreenY);

	//set the initial scale
	m_fScale		=	(LTFLOAT)m_nScreenX/m_nHalfImageWidth;

	//now calculate the actual image halves
	m_nHalfImageWidth /= 2;
	m_nHalfImageHeight /= 2;

	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
	pStats->SetAutoTargetOn(LTTRUE);

	//set our start time
	m_fTime = 0.0f;

	//play the triangel sounds
	g_pClientSoundMgr->PlaySoundLocal("pred_lasertrack");

	return DTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPredTargetSFX::WantRemove
//
//	PURPOSE:	If this gets called, remove the target (this should only get
//				called if we have a server object associated with us, and 
//				that server object gets removed).
//
// ----------------------------------------------------------------------- //

void CPredTargetSFX::WantRemove(DBOOL bRemove)
{
	CSpecialFX::WantRemove(bRemove);

	if (!g_pGameClientShell) return;

	CSFXMgr* psfxMgr = g_pGameClientShell->GetSFXMgr();
	if (!psfxMgr) return;

	// Tell the special fx mgr to go ahead and remove us...

	if (m_hObject)
	{
		psfxMgr->RemoveSpecialFX(m_hObject);
	}
}
// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPredTargetSFX::UpdatePhase
//
//	PURPOSE:	Update the targetign phase
//
// ----------------------------------------------------------------------- //

TargetPhase CPredTargetSFX::UpdatePhase()
{
	if(m_fTime > (g_cvarSegmentDelay.GetFloat() *2 + g_cvarSegmentTime.GetFloat()) )
	{
		//play the lock sound
		g_pClientSoundMgr->PlaySoundLocal("pred_lockon");
		m_hLockSound = g_pClientSoundMgr->PlaySoundLocal("pred_lockloop");
		return TP_LOCKED;
	}

	else if(m_fTime < g_cvarSegmentDelay.GetFloat() )
		return TP_LOCKING_0;

	else if(m_fTime < g_cvarSegmentDelay.GetFloat() * 2 )
		return TP_LOCKING_1;

	else return TP_LOCKING_2;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPredTargetSFX::Update
//
//	PURPOSE:	Update function
//
// ----------------------------------------------------------------------- //

LTBOOL CPredTargetSFX::Update()
{
	if(g_pGameClientShell->GetPlayerStats()->GetHealthCount() == 0)
		return LTFALSE;

	LTIntPt ptTemp;

	//update the screen position of the target
	GetScreenPos(	g_pGameClientShell->GetPlayerCamera(), 
					g_pInterfaceMgr->GetPlayerStats()->GetAutoTarget(),
					ptTemp, LTTRUE);

	m_fXPos = (float)ptTemp.x;
	m_fYPos = (float)ptTemp.y;

	//update time
	m_fTime += g_pLTClient->GetFrameTime();

	//update the phase
	if(m_ePhase != TP_LOCKED)
		m_ePhase = UpdatePhase();
	else
	{
		//update the angle of the image's rotation
		LTFLOAT fFrameTime = g_pLTClient->GetFrameTime();
		m_fAngle +=  fFrameTime * g_cvarRotRate.GetFloat();
		
		if(m_fAngle > MATH_PI*2)
			m_fAngle = 0.0;
	}

	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();

	return (LTBOOL)pStats->GetAutoTarget();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPredTargetSFX::PostRenderDraw
//
//	PURPOSE:	Handles the 2d drawing
//
// ----------------------------------------------------------------------- //

void CPredTargetSFX::PostRenderDraw()
{
	// In stereo they're drawn into the eye views instead (DrawInEyes): on the head-locked HUD
	// panel they don't stay on the target as the head turns
	if(g_pGameClientShell->GetVRMgr()->IsStereoRendered())
		return;

	if(g_pGameClientShell->IsFirstPerson())
	{
		LTRect rScreen;
		rScreen.left = rScreen.top = 0;
		rScreen.right = (int)m_nScreenX;
		rScreen.bottom = (int)m_nScreenY;
		DrawAt(rScreen, m_fXPos, m_fYPos, 1.0f, 1.0f);
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPredTargetSFX::DrawInEyes
//
//	PURPOSE:	VR: draws the triangles at the target in each eye's view
//
// ----------------------------------------------------------------------- //

void CPredTargetSFX::DrawInEyes()
{
	HOBJECT hTarget = g_pInterfaceMgr->GetPlayerStats()->GetAutoTarget();
	if(!hTarget || !g_pGameClientShell->IsFirstPerson())
		return;

	LTVector vTarget = GetScreenTargetPos(hTarget, LTTRUE);
	VRMgr *pVR = g_pGameClientShell->GetVRMgr();
	for(int nEye = 0; nEye < 2; nEye++)
	{
		LTFLOAT fX, fY, fScaleX, fScaleY;
		LTRect rEye;
		if(!pVR->ProjectToEye(nEye, vTarget, fX, fY, fScaleX, fScaleY, rEye))
			continue;
		// avp2xr 11 on draws them at the headset's resolution; else they're drawn here, in the
		// game's pixels (blocky in the headset)
		m_nMarkerEye = pVR->CanAddMarkers() ? nEye : -1;
		DrawAt(rEye, fX, fY, fScaleX, fScaleY);
		m_nMarkerEye = -1;
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPredTargetSFX::PreloadVRImages
//
//	PURPOSE:	VR: gets avp2xr's copies of the images ready (see the header)
//
// ----------------------------------------------------------------------- //

void CPredTargetSFX::PreloadVRImages()
{
	if(!g_pGameClientShell->GetVRMgr()->CanAddMarkers() || !g_pInterfaceResMgr)
		return;

	char *szImages[4] = { s_szLockedImage, s_szTargetingImage[0], s_szTargetingImage[1], s_szTargetingImage[2] };
	for(int i = 0; i < 4; i++)
	{
		HSURFACE hImage = g_pInterfaceResMgr->GetSharedSurface(szImages[i]);
		const uint32 *pPixels;
		uint32 nWidth, nHeight, nId;
		if(hImage)
			CCrosshairMgr::GetVRImage(szImages[i], hImage, LTTRUE, pPixels, nWidth, nHeight, nId);
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPredTargetSFX::DrawAt
//
//	PURPOSE:	Draws the triangles in a view with the target at fX, fY
//
// ----------------------------------------------------------------------- //

void CPredTargetSFX::DrawAt(const LTRect &rView, LTFLOAT fX, LTFLOAT fY, LTFLOAT fScaleX, LTFLOAT fScaleY)
{
	switch(m_ePhase)
	{
	case(TP_LOCKED):
		{
			if(m_nMarkerEye >= 0)
			{
				uint32 nImageW = 0, nImageH = 0;
				g_pLTClient->GetSurfaceDims(m_hLockedImage, &nImageW, &nImageH);
				g_pGameClientShell->GetVRMgr()->AddMarker(s_szLockedImage, m_hLockedImage, m_nMarkerEye, fX, fY,
					nImageW * 0.5f * fScaleX, nImageH * 0.5f * fScaleY, m_fAngle, 0.5f);
				break;
			}

			if(fScaleX == 1.0f && fScaleY == 1.0f)
			{
				// Rotated about the image's centre, so its top-left at the half size keeps it
				// centred on the target
				g_pLTClient->TransformSurfaceToSurfaceTransparent
					(	g_pClientDE->GetScreenSurface(),
						m_hLockedImage,
						LTNULL,
						(int)fX-m_nHalfImageWidth, (int)fY-m_nHalfImageHeight,
						m_fAngle,
						1.0f,
						1.0f,
						SETRGB_T(0,0,0));
				break;
			}

			// In an eye the game's pixels aren't square, so the image is scaled differently across
			// and down. TransformSurfaceToSurface doesn't do that right (the image came out
			// stretched across), so its corners are turned here, then scaled, and it's warped to them
			uint32 nImageW = 0, nImageH = 0;
			g_pLTClient->GetSurfaceDims(m_hLockedImage, &nImageW, &nImageH);
			LTFLOAT fCos = (LTFLOAT)cos(m_fAngle), fSin = (LTFLOAT)sin(m_fAngle);
			LTWarpPt pts[4];
			for(int i = 0; i < 4; i++)
			{
				LTFLOAT u = (i == 1 || i == 2) ? 1.0f : 0.0f;	// corners clockwise from the top-left
				LTFLOAT v = (i >= 2) ? 1.0f : 0.0f;
				LTFLOAT x = (u - 0.5f) * nImageW, y = (v - 0.5f) * nImageH;
				pts[i].source_x = u * nImageW;
				pts[i].source_y = v * nImageH;
				pts[i].dest_x = fX + (x * fCos - y * fSin) * fScaleX;
				pts[i].dest_y = fY + (x * fSin + y * fCos) * fScaleY;
			}
			g_pLTClient->WarpSurfaceToSurfaceTransparent(g_pClientDE->GetScreenSurface(), m_hLockedImage, pts, 4, SETRGB_T(0,0,0));
			break;
		}
	default:
		DrawLockingTris(rView, fX, fY, fScaleX, fScaleY);
		break;
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPredTargetSFX::DrawClipped
//
//	PURPOSE:	Draws an image scaled to a rectangle centred on fCX, fCY, cut to
//				rView (so in stereo it stays inside its eye's half of the screen)
//
// ----------------------------------------------------------------------- //

void CPredTargetSFX::DrawClipped(int nImage, LTFLOAT fCX, LTFLOAT fCY, LTFLOAT fHalfW, LTFLOAT fHalfH, const LTRect &rView)
{
	HSURFACE hImage = m_hTargetingImage[nImage];

	// avp2xr cuts it to the eye itself
	if(m_nMarkerEye >= 0)
	{
		g_pGameClientShell->GetVRMgr()->AddMarker(s_szTargetingImage[nImage], hImage, m_nMarkerEye, fCX, fCY,
			fHalfW, fHalfH, 0.0f, 0.5f);
		return;
	}

	uint32 nImageW = 0, nImageH = 0;
	g_pLTClient->GetSurfaceDims(hImage, &nImageW, &nImageH);
	if(!nImageW || !nImageH || fHalfW <= 0.0f || fHalfH <= 0.0f)
		return;

	LTFLOAT fLeft = fCX - fHalfW, fTop = fCY - fHalfH, fRight = fCX + fHalfW, fBottom = fCY + fHalfH;
	LTRect rcDest;
	rcDest.left = (int)(fLeft > rView.left ? fLeft : rView.left);
	rcDest.top = (int)(fTop > rView.top ? fTop : rView.top);
	rcDest.right = (int)(fRight < rView.right ? fRight : rView.right);
	rcDest.bottom = (int)(fBottom < rView.bottom ? fBottom : rView.bottom);
	if(rcDest.right <= rcDest.left || rcDest.bottom <= rcDest.top)
		return;

	// The part of the image that's left
	LTFLOAT fPerX = nImageW / (fRight - fLeft), fPerY = nImageH / (fBottom - fTop);
	LTRect rcSrc;
	rcSrc.left = (int)((rcDest.left - fLeft) * fPerX);
	rcSrc.top = (int)((rcDest.top - fTop) * fPerY);
	rcSrc.right = (int)((rcDest.right - fLeft) * fPerX + 0.5f);
	rcSrc.bottom = (int)((rcDest.bottom - fTop) * fPerY + 0.5f);
	if(rcSrc.right > (int)nImageW) rcSrc.right = (int)nImageW;
	if(rcSrc.bottom > (int)nImageH) rcSrc.bottom = (int)nImageH;
	if(rcSrc.right <= rcSrc.left || rcSrc.bottom <= rcSrc.top)
		return;

	g_pLTClient->ScaleSurfaceToSurfaceTransparent(g_pClientDE->GetScreenSurface(), hImage, &rcDest, &rcSrc, SETRGB_T(0,0,0));
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPredTargetSFX::DrawLockingTris
//
//	PURPOSE:	Handles locking segments: the triangles come in from the
//				bottom, left and right edges of the view to the target
//
// ----------------------------------------------------------------------- //

void CPredTargetSFX::DrawLockingTris(const LTRect &rView, LTFLOAT fX, LTFLOAT fY, LTFLOAT fScaleX, LTFLOAT fScaleY)
{
	LTFLOAT fMidX = (rView.left + rView.right) * 0.5f;
	LTFLOAT fMidY = (rView.top + rView.bottom) * 0.5f;
	LTFLOAT fHalfW = m_nHalfImageWidth * fScaleX;
	LTFLOAT fHalfH = m_nHalfImageHeight * fScaleY;

	if(m_ePhase >= TP_LOCKING_0)
	{
		//draw the base tri

		//calc the percentage of travel
		LTFLOAT fRatio = m_fTime / g_cvarSegmentTime.GetFloat();
		if(fRatio > 1.0f) fRatio = 1.0f;
		LTFLOAT fScale = 1+(1-fRatio)*m_fScale;
		DrawClipped(0, fMidX + fRatio*(fX - fMidX), rView.bottom + fRatio*(fY - rView.bottom),
					fHalfW*fScale, fHalfH*fScale, rView);
	}
	if(m_ePhase >= TP_LOCKING_1)
	{
		//draw the left tri

		//calc the percentage of travel
		LTFLOAT fRatio = (m_fTime-g_cvarSegmentDelay.GetFloat()) / g_cvarSegmentTime.GetFloat();
		if(fRatio > 1.0f) fRatio = 1.0f;
		LTFLOAT fScale = 1+(1-fRatio)*m_fScale;
		DrawClipped(1, rView.left + fRatio*(fX - rView.left), fMidY + fRatio*(fY - fMidY),
					fHalfW*fScale, fHalfH*fScale, rView);
	}
	if(m_ePhase >= TP_LOCKING_2)
	{
		//draw the right tri

		//calc the percentage of travel
		LTFLOAT fRatio = (m_fTime-(g_cvarSegmentDelay.GetFloat()*2)) / g_cvarSegmentTime.GetFloat();
		if(fRatio > 1.0f) fRatio = 1.0f;
		LTFLOAT fScale = 1+(1-fRatio)*m_fScale;
		DrawClipped(2, rView.right + fRatio*(fX - rView.right), fMidY + fRatio*(fY - fMidY),
					fHalfW*fScale, fHalfH*fScale, rView);
	}
}


//-------------------------------------------------------------------------------------------------
// SFX_PredTargetFactory
//-------------------------------------------------------------------------------------------------

const SFX_PredTargetFactory SFX_PredTargetFactory::registerThis;

CSpecialFX* SFX_PredTargetFactory::MakeShape() const
{
	return new CPredTargetSFX();
}

