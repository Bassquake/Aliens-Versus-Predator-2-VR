//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrackBarCamera.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_BAR_CAMERA_H_
#define _LITHTECH_TRACK_BAR_CAMERA_H_

#ifdef _CLIENTBUILD

//*********************************************************************************

#include "lithtrackbarbase.h"

//*********************************************************************************

#define CAMERABAR_TEXT_TITLE						0
#define CAMERABAR_TEXT_POSITION						1
#define CAMERABAR_TEXT_ROTATION						2
#define CAMERABAR_TEXT_FOV							3
#define CAMERABAR_TEXT_LOCK_POS						4
#define CAMERABAR_TEXT_LOCK_ROT						5

#define CAMERABAR_BUTTON_CONTROL					0
#define CAMERABAR_BUTTON_RESET						1
#define CAMERABAR_BUTTON_LOCK_POS_X					2
#define CAMERABAR_BUTTON_LOCK_POS_Y					3
#define CAMERABAR_BUTTON_LOCK_POS_Z					4
#define CAMERABAR_BUTTON_LOCK_ROT_X					5
#define CAMERABAR_BUTTON_LOCK_ROT_Y					6
#define CAMERABAR_BUTTON_LOCK_ROT_Z					7

//*********************************************************************************

class LithTrackBarCamera : public LithTrackBarBase
{
	public:
		// Initialization and termination functions
		DBOOL			Init(INTERFACE *pInterface = DNULL, LithFont *pFont = DNULL, void *pData = DNULL);

		// GUI update functions
		DBOOL			Update();
		DBOOL			UpdateResolution(DDWORD nWidth, DDWORD nHeight);
		DBOOL			UpdateItems();

		// Mouse input functions
		void			OnMouseClick();

		// Special control functions
		void			OnEditModeOn();
		void			OnControlChange();

	protected:
		void			SetupLines(int nWidth, int nHeight);

	protected:
		DATA_CAMERA		m_dCamera;
};

//*********************************************************************************

#endif // _CLIENTBUILD

#endif // _LITHTECH_TRACK_BAR_CAMERA_H_
