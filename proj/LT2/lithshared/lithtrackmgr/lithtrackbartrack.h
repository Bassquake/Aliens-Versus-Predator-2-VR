//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrackBarTrack.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_BAR_TRACK_H_
#define _LITHTECH_TRACK_BAR_TRACK_H_

#ifdef _CLIENTBUILD

//*********************************************************************************

#include "lithtrackbarbase.h"

//*********************************************************************************

#define TRACKBAR_TEXT_TITLE							0
#define TRACKBAR_TEXT_TRACK							1
#define TRACKBAR_TEXT_TRACK_NUM						2
#define TRACKBAR_TEXT_TRACK_NAME					3
#define TRACKBAR_TEXT_TRACK_POSITION				4
#define TRACKBAR_TEXT_TRACK_ROTATION				5
#define TRACKBAR_TEXT_LOCK_POS						6
#define TRACKBAR_TEXT_LOCK_ROT						7

#define TRACKBAR_BUTTON_DISPLAY_TRACK				0
#define TRACKBAR_BUTTON_CONTROL_TRACK				1

#define TRACKBAR_BUTTON_NEW_TRACK					2
#define TRACKBAR_BUTTON_COPY_TRACK					3
#define TRACKBAR_BUTTON_DELETE_TRACK				4
#define TRACKBAR_BUTTON_ALIGN_TRACK					5
#define TRACKBAR_BUTTON_RESET						6
#define TRACKBAR_BUTTON_LOCK_POS_X					7
#define TRACKBAR_BUTTON_LOCK_POS_Y					8
#define TRACKBAR_BUTTON_LOCK_POS_Z					9
#define TRACKBAR_BUTTON_LOCK_ROT_X					10
#define TRACKBAR_BUTTON_LOCK_ROT_Y					11
#define TRACKBAR_BUTTON_LOCK_ROT_Z					12

#define TRACKBAR_BUTTON_PREV_TRACK_FAST				13
#define TRACKBAR_BUTTON_PREV_TRACK					14
#define TRACKBAR_BUTTON_SCROLL_TRACK				15
#define TRACKBAR_BUTTON_NEXT_TRACK					16
#define TRACKBAR_BUTTON_NEXT_TRACK_FAST				17

//*********************************************************************************

class LithTrackBarTrack : public LithTrackBarBase
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
		void			OnEditModeOff();
		void			OnControlChange();

	protected:
		void			SetupLines(int nWidth, int nHeight);
		void			UpdateControl();

	protected:
		int					m_nCurrentTrack;				// Current
		DBOOL				m_bResetOnUpdate;				// Should we reset the surface at the end of the update?
		DBOOL				m_bFirstDisplay;				// Is it the first time we hit the display button?
};

//*********************************************************************************

#endif // _CLIENTBUILD

#endif // _LITHTECH_TRACK_GUI_BAR_H_
