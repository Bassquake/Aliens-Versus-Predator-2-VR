// ----------------------------------------------------------------------- //
//
// MODULE  : PVFXMgr.h
//
// PURPOSE : Player-view fx manager - Definition
//
// CREATED : 12/13/99
//
// (c) 1999-2000 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#ifndef __PV_FX_MGR_H__
#define __PV_FX_MGR_H__

#include "clientheaders.h"
#include "BaseScaleFX.h"
#include "DynamicLightFX.h"
#include "SoundFX.h"

struct WEAPON;
struct CScaleFX;
struct DLIGHTFX;
struct SOUNDFX;

#define PVFX_MAX_SCALE_FX	10
#define	PVFX_MAX_DLIGHT_FX	10
#define PVFX_MAX_SOUND_FX	10

struct PVFX_STRUCT
{
	PVFX_STRUCT()
	{
        pFX     = LTNULL;
        bUsed   = LTFALSE;
        bOn     = LTFALSE;
		hSocket	= INVALID_MODEL_SOCKET;
        pName   = LTNULL;
	}

	void Term()
	{
		if (pFX) pFX->Term();

        bOn = LTFALSE;
        bUsed = LTFALSE;
		hSocket = INVALID_MODEL_SOCKET;
        pName = LTNULL;
	}

	CSpecialFX*		pFX;
    LTBOOL          bUsed;
    LTBOOL          bOn;
	HMODELSOCKET	hSocket;
	char*			pName;
};

class CPVFXMgr
{
	public :

		CPVFXMgr();
		~CPVFXMgr();

		void	Term();
        LTBOOL	Init(HOBJECT hModelObj, WEAPON* pWeapon);

		void	HandleFXKey(ArgList* pArgList);
		void	Update();

		// VR: after the (really-close) model has been placed in the current eye's view, puts the
		// scale fx at its sockets again, and the lights at the sockets' place in the world
		// (vEyePos/rEyeRot: the eye camera; fScale: how much bigger the model is drawn).
		void	PlaceForView(const LTVector &vEyePos, const LTRotation &rEyeRot, LTFLOAT fScale);

		void	SetVisible(LTBOOL bVis = LTTRUE);

	private :

		void DisableAllFX();
        void TurnOn(char* pFXName, LTBOOL bOn);
		void AddScaleFX(CScaleFX* pScaleFX, HMODELSOCKET hSocket, char* pName);
		void AddDLightFX(DLIGHTFX* pDLightFX, HMODELSOCKET hSocket, char* pName);
		void AddSoundFX(SOUNDFX* pSoundFX, char* pName);
		void Toggle(char* pFXName);

		HOBJECT		m_hModelObject;
		LTBOOL		m_bVisible;

		// These arrays hold the actual data...

		CBaseScaleFX	m_prvtScaleFX[PVFX_MAX_SCALE_FX];
		CDynamicLightFX	m_prvtLightFX[PVFX_MAX_DLIGHT_FX];
		CSoundFX		m_prvtSoundFX[PVFX_MAX_SOUND_FX];

		// These arrays are what we access (hold above data, plus
		// some extra info...
		PVFX_STRUCT		m_ScaleFX[PVFX_MAX_SCALE_FX];
		PVFX_STRUCT		m_DLightFX[PVFX_MAX_DLIGHT_FX];
		PVFX_STRUCT		m_SoundFX[PVFX_MAX_SOUND_FX];
};

#endif // __PV_FX_MGR_H__