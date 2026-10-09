 // ----------------------------------------------------------------------- //
//
// MODULE  : PredTargetFx.h
//
// PURPOSE : Predator Target FX class - Definition
//
// CREATED : 6/13/2000
//
// ----------------------------------------------------------------------- //

#ifndef __PRED_TARGET_FX_H__
#define __PRED_TARGET_FX_H__

#include "SpecialFX.h"
#include "dlink.h"

enum TargetPhase
{
	TP_LOCKING_0 = 0,
	TP_LOCKING_1,
	TP_LOCKING_2,
	TP_LOCKED,
};

class CPredTargetSFX : public CSpecialFX
{
	public :

		CPredTargetSFX()
		{
			m_hImage			= LTNULL;
			m_fXPos				= 0.0f;
			m_fYPos				= 0.0f;
			m_fScale			= 0.0f;;
			m_fAngle			= 0.0f;;
			m_hLockedImage		= LTNULL;
			m_nHalfImageWidth	= 0;
			m_nHalfImageHeight	= 0;
			m_nScreenX			= 0;
			m_nScreenY			= 0;
			m_ePhase			= TP_LOCKING_0;
			m_fTime				= 0.0f;
			m_hLockSound		= LTNULL;
			m_nMarkerEye		= -1;
			memset(m_hTargetingImage, 0, sizeof(HSURFACE)*3);
		}
		~CPredTargetSFX()
		{
			//surfaces do not need to be free'd since they are
			//being managed by the interface surface manager.

			if(m_hLockSound)
			{
				g_pLTClient->KillSound(m_hLockSound);
				m_hLockSound = LTNULL;
			}
		}

		LTBOOL			Init(HLOCALOBJ hServObj, HMESSAGEREAD hRead);
		LTBOOL			Init(SFXCREATESTRUCT* psfxCreateStruct);
		LTBOOL			Update();
		LTBOOL			CreateObject(CClientDE* pClientDE);
		TargetPhase	UpdatePhase();

		void	WantRemove(LTBOOL bRemove=DTRUE);
		void	PostRenderDraw();

		// VR: the target triangles drawn into each eye's view at the target (see VRMgr::DrawEyeOverlays);
		// PostRenderDraw then leaves them off the HUD
		void	DrawInEyes();

		// VR: makes avp2xr's copies of the images now (about 70 ms each, reading the pixels one at a
		// time), so the first lock doesn't hitch. Called when the player becomes a Predator.
		static void	PreloadVRImages();

	private :
		// Draws the triangles in rView (the screen, or one eye's part of it) with the target at
		// fX, fY and the images fScaleX, fScaleY times their flat-screen size
		void	DrawAt(const LTRect &rView, LTFLOAT fX, LTFLOAT fY, LTFLOAT fScaleX, LTFLOAT fScaleY);
		void	DrawLockingTris(const LTRect &rView, LTFLOAT fX, LTFLOAT fY, LTFLOAT fScaleX, LTFLOAT fScaleY);
		void	DrawClipped(int nImage, LTFLOAT fCX, LTFLOAT fCY, LTFLOAT fHalfW, LTFLOAT fHalfH, const LTRect &rView);

		int		m_nMarkerEye;	// VR with avp2xr 11 on: the eye DrawAt is giving avp2xr markers for, else -1 (drawn here)

		HSURFACE	m_hLockedImage;
		HSURFACE	m_hTargetingImage[3];
		HSURFACE	m_hImage;
		LTFLOAT		m_fXPos;
		LTFLOAT		m_fYPos;
		LTFLOAT		m_fScale;
		LTFLOAT		m_fAngle;
		uint32		m_nHalfImageWidth;
		uint32		m_nHalfImageHeight;
		uint32		m_nScreenX;
		uint32		m_nScreenY;
		TargetPhase	m_ePhase;
		LTFLOAT		m_fTime;
		HLTSOUND	m_hLockSound;
};

//-------------------------------------------------------------------------------------------------
// SFX_PredTargetFactory
//-------------------------------------------------------------------------------------------------
#ifndef C_SPECIAL_FX_FACTORY_H
#include "CSpecialFXFactory.h"
#endif

#ifndef __SFX_MSG_IDS_H__
#include "SFXMsgIds.h"
#endif

class SFX_PredTargetFactory : public CSpecialFXFactory
{
	SFX_PredTargetFactory() : CSpecialFXFactory(SFX_PRED_TARGET_ID) {;}
	static const SFX_PredTargetFactory registerThis;

	virtual CSpecialFX* MakeShape() const;
};

#endif // __PRED_TARGET_FX_H__