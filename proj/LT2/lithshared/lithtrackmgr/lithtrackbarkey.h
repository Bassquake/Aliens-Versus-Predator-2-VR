//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrackBarKey.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_BAR_KEY_H_
#define _LITHTECH_TRACK_BAR_KEY_H_

#ifdef _CLIENTBUILD

//*********************************************************************************

#include "lithtrackbarbase.h"

//*********************************************************************************

class LithTrack;

//*********************************************************************************

#define KEYBAR_TEXT_TITLE							0
#define KEYBAR_TEXT_KEY								1
#define KEYBAR_TEXT_KEY_NUM							2
#define KEYBAR_TEXT_KEY_FLEX						3
#define KEYBAR_TEXT_KEY_POSITION					4
#define KEYBAR_TEXT_KEY_ROTATION					5
#define KEYBAR_TEXT_LOCK_POS						6
#define KEYBAR_TEXT_LOCK_ROT						7
#define KEYBAR_TEXT_FLEX							8
#define KEYBAR_TEXT_TIME							9

#define KEYBAR_BUTTON_CONTROL_KEY					0

#define KEYBAR_BUTTON_INSERT_KEY					1
#define KEYBAR_BUTTON_DELETE_KEY					2
#define KEYBAR_BUTTON_RESET_KEY						3
#define KEYBAR_BUTTON_LOCK_POS_X					4
#define KEYBAR_BUTTON_LOCK_POS_Y					5
#define KEYBAR_BUTTON_LOCK_POS_Z					6
#define KEYBAR_BUTTON_LOCK_ROT_X					7
#define KEYBAR_BUTTON_LOCK_ROT_Y					8
#define KEYBAR_BUTTON_LOCK_ROT_Z					9

#define KEYBAR_BUTTON_PREV_KEY_FAST					10
#define KEYBAR_BUTTON_PREV_KEY						11
#define KEYBAR_BUTTON_SCROLL_KEY					12
#define KEYBAR_BUTTON_NEXT_KEY						13
#define KEYBAR_BUTTON_NEXT_KEY_FAST					14

//*********************************************************************************

class LithTrackBarKey : public LithTrackBarBase
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
		void			OnControlChange();

	protected:
		void			SetupLines(int nWidth, int nHeight);
		void			UpdateControl();

	protected:
		LithTrack			*m_pLastTrack;					// The last track we were drawing
		int					m_nCurrentKey;					// Current
		DBOOL				m_bResetOnUpdate;				// Should we reset the surface at the end of the update?
};

//*********************************************************************************

#endif // _CLIENTBUILD

#endif // _LITHTECH_TRACK_GUI_BAR_H_
